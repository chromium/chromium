// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITED_URL_RANKING_URL_DEDUPLICATION_SEARCH_ENGINE_URL_STRIP_HANDLER_H_
#define CHROME_BROWSER_VISITED_URL_RANKING_URL_DEDUPLICATION_SEARCH_ENGINE_URL_STRIP_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/url_deduplication/url_strip_handler.h"

class GURL;
class SearchTermsData;
class TemplateURL;
class TemplateURLService;

class SearchEngineURLStripHandler : public URLStripHandler {
 public:
  SearchEngineURLStripHandler(TemplateURLService* template_url_service,
                              bool keep_search_intent_params,
                              bool normalize_search_terms,
                              std::u16string keyword);

  SearchEngineURLStripHandler(const SearchEngineURLStripHandler&) = delete;
  SearchEngineURLStripHandler& operator=(const SearchEngineURLStripHandler&) =
      delete;

  ~SearchEngineURLStripHandler();
  // URLStripHandler:
  GURL StripExtraParams(GURL);

 private:
  // Returns a SearchTermsData which can be used to call TemplateURL methods.
  const SearchTermsData& search_terms_data() const;

  // A version of AutocompleteMatch::GetTemplateURL() that takes the a keyword
  // and a hostname as parameters.  In short, returns the TemplateURL associated
  // with |keyword| if it exists; otherwise returns the TemplateURL associated
  // with |host| if it exists.
  TemplateURL* GetTemplateURLWithKeyword(const std::u16string& keyword,
                                         const std::string& host);
  const TemplateURL* GetConstTemplateURLWithKeyword(
      const std::u16string& keyword,
      const std::string& host) const;

  // Returns true if |url| supports replacement.
  bool SupportsReplacement(const std::u16string& keyword,
                           const std::string& host,
                           const SearchTermsData& search_terms_data) const;

  // Given a `url` corresponding to this TemplateURL, keeps the search terms and
  // optionally the search intent params and removes the other params. If
  // `normalize_search_terms` is true, the search terms in the final URL
  // will be converted to lowercase with extra whitespace characters collapsed.
  // If `url` is not a search URL or replacement fails, leaves `out_url` and
  // `out_search_terms` untouched and returns false. Used to compare
  // normalized (aka canonical) search URLs.
  bool KeepSearchTermsInURL(const std::u16string& keyword,
                            const std::string& host,
                            const GURL& url,
                            const SearchTermsData& search_terms_data,
                            const bool keep_search_intent_params,
                            const bool normalize_search_terms,
                            GURL* out_url,
                            std::u16string* out_search_terms = nullptr) const;

  const raw_ptr<TemplateURLService> template_url_service_;
  bool keep_search_intent_params_;
  bool normalize_search_terms_;
  std::u16string keyword_;
};

#endif  // CHROME_BROWSER_VISITED_URL_RANKING_URL_DEDUPLICATION_SEARCH_ENGINE_URL_STRIP_HANDLER_H_
