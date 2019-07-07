#include "stdafx.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <curl/curl.h>
#include <tidy/tidy.h>
#include <tidy/buffio.h>
#include "tinyxml2.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"


using namespace rapidjson;

std::string CleanHTML(const std::string &html) {
	// Initialize a Tidy document
	TidyDoc tidyDoc = tidyCreate();
	TidyBuffer tidyOutputBuffer = { 0 };

	bool configSuccess = tidyOptSetBool(tidyDoc, TidyXmlOut, yes)
		&& tidyOptSetBool(tidyDoc, TidyQuiet, yes)
		&& tidyOptSetBool(tidyDoc, TidyShowInfo, no)
		&& tidyOptSetBool(tidyDoc, TidyNumEntities, yes)
		&& tidyOptSetBool(tidyDoc, TidyShowWarnings, no);

	int tidyResponseCode = -1;

	// Parse input
	if (configSuccess)
		tidyResponseCode = tidyParseString(tidyDoc, html.c_str());

	// Process HTML
	if (tidyResponseCode >= 0)
		tidyResponseCode = tidyCleanAndRepair(tidyDoc);

	// Output the HTML to our buffer
	if (tidyResponseCode >= 0)
		tidyResponseCode = tidySaveBuffer(tidyDoc, &tidyOutputBuffer);

	// Any errors from Tidy?
	if (tidyResponseCode < 0)
		throw ("Tidy encountered an error while parsing an HTML response. Tidy response code: " + tidyResponseCode);

	// Grab the result from the buffer and then free Tidy's memory
	std::string tidyResult = (char*)tidyOutputBuffer.bp;
	tidyBufFree(&tidyOutputBuffer);
	tidyRelease(tidyDoc);

	return tidyResult;
}

size_t CurlWrite_CallbackFunc(void *contents, size_t size, size_t nmemb, std::string *s) {
	size_t newLength = size * nmemb;
	try
	{
		s->append((char*)contents, newLength);
		static int i = 0;
		
		if (i != 0) std::cout << "\b";
		switch (i++ % 4) {
		case 0:
			std::cout << "|";
			break;
		case 1:
			std::cout << "/";
			break;
		case 2:
			std::cout << "-";
			break;
		case 3:
			std::cout << "\\";
			break;
		}
	}
	catch (std::bad_alloc &)
	{
		//handle memory problem
		return 0;
	}
	return newLength;
}

tinyxml2::XMLElement* traverse(tinyxml2::XMLElement* elem, const std::string& searchText, bool prn = false) {
	const static std::string head = "head";
	if (!elem) {
		return nullptr;
	}
	else {
		if (elem->Value() && !head.compare(elem->Value())) {
			return traverse(elem->NextSiblingElement(), searchText);
		}
		if (elem->GetText() && !searchText.compare(elem->GetText())) {
			return elem;
		}
		if (prn) {
			if (elem->GetText()) std::cout << "Text: " << elem->GetText() << '\n';
			if (elem->Value()) std::cout << "Value: " << elem->Value() << '\n';
		}
	}
	for (auto child = elem->FirstChildElement(); child; child = child->NextSiblingElement()) {
		auto x = traverse(child, searchText);
		if (x && x->GetText() && !searchText.compare(x->GetText())) {
			return x;
		}
	}
	if (elem->NextSiblingElement()) return traverse(elem->NextSiblingElement(), searchText);
	return nullptr;
}

int main(void) {
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	std::string htmlText;

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &htmlText);
		// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); 
		curl_easy_setopt(curl, CURLOPT_URL, "https://time.com/");
		res = curl_easy_perform(curl);
		std::cout << "\b";
		if (res != CURLE_OK) {
			std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
		}
		curl_easy_cleanup(curl);
	}
	std::string tidyHTML = CleanHTML(htmlText);

	tinyxml2::XMLDocument doc;
	doc.Parse(tidyHTML.c_str());
	tinyxml2::XMLElement* elem = doc.FirstChildElement();
	auto searchElem = traverse(elem, "The Brief");
	searchElem = searchElem->NextSiblingElement();
	if (searchElem) {
		const char* json = "{\"news\":[]}";
		Document d;
		d.Parse(json);
		Value& arrJson = d["news"];
		for (int i = 0; i < 6; i++) {
#if 1
			Value newsItem(kObjectType);
			auto c1 = searchElem->FirstChildElement();
			if (i == 0) {
				c1 = c1->NextSiblingElement();
			}
			if (c1) {
				auto c2 = c1->FirstChildElement();
				if (c2) {
					auto c3 = c2->FirstChildElement();
					if (c3) {
						auto c4 = c3->FirstChildElement();
						if (c4) {
							std::string text = c4->GetText();
							std::string val = c4->Attribute("href");
							std::replace(text.begin(), text.end(), '\n', ' ');
							
							newsItem.AddMember(StringRef("title"), rapidjson::Value(text.c_str(), d.GetAllocator()).Move(), d.GetAllocator());
							newsItem.AddMember(StringRef("link"), rapidjson::Value((std::string("https://time.com/") + val).c_str(), d.GetAllocator()).Move(), d.GetAllocator());
							// newsItem.AddMember(StringRef("title"), StringRef(text.c_str()), d.GetAllocator());
							// newsItem.AddMember(StringRef("link"), StringRef((std::string("https://time.com/") + val).c_str()), d.GetAllocator());
							arrJson.PushBack(newsItem, d.GetAllocator());
#if 0
							std::cout << "Text: " << text << '\n';
							std::cout << "Value: " << val << '\n';
#endif
						}
					}
				}
			}
#endif
			searchElem = searchElem->NextSiblingElement();
		}


		StringBuffer strbuf;
		PrettyWriter<StringBuffer> writer(strbuf);
		d.Accept(writer);

		std::cout << strbuf.GetString() << std::endl;


	} else {
		std::cerr << "Error getting \"The Brief\" news items from time.com\n";
	}
	return 0;
}
