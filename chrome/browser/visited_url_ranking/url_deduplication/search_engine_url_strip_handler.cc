// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/url_deduplication/search_engine_url_strip_handler.h"

#include <string>

#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

SearchEngineURLStripHandler::SearchEngineURLStripHandler(
    TemplateURLService* template_url_service,
    bool keep_search_intent_params,
    bool normalize_search_terms,
    std::u16string keyword)
    : template_url_service_(template_url_service),
      keep_search_intent_params_(keep_search_intent_params),
      normalize_search_terms_(normalize_search_terms),
      keyword_(keyword) {}

SearchEngineURLStripHandler::~SearchEngineURLStripHandler() = default;

GURL SearchEngineURLStripHandler::StripExtraParams(GURL url) {
  if (!url.is_valid()) {
    return url;
  }

  GURL stripped_destination_url = url;

  if (template_url_service_) {
    // If the destination URL looks like it was generated from a TemplateURL,
    // remove all substitutions other than the search terms and optionally the
    // search intent params. This allows eliminating cases like past search URLs
    // from history that differ only by some obscure query param from each other
    // or from the search/keyword provider matches.
    if (SupportsReplacement(keyword_, stripped_destination_url.host(),
                            search_terms_data())) {
      KeepSearchTermsInURL(keyword_, stripped_destination_url.host(), url,
                           search_terms_data(), keep_search_intent_params_,
                           normalize_search_terms_, &stripped_destination_url);
    }
  }

  return stripped_destination_url;
}

// Returns a SearchTermsData which can be used to call TemplateURL methods.
const SearchTermsData& SearchEngineURLStripHandler::search_terms_data() const {
  return template_url_service_->search_terms_data();
}

TemplateURL* SearchEngineURLStripHandler::GetTemplateURLWithKeyword(
    const std::u16string& keyword,
    const std::string& host) {
  return const_cast<TemplateURL*>(
      GetConstTemplateURLWithKeyword(keyword, host));
}

const TemplateURL* SearchEngineURLStripHandler::GetConstTemplateURLWithKeyword(
    const std::u16string& keyword,
    const std::string& host) const {
  const TemplateURL* template_url =
      keyword.empty()
          ? nullptr
          : template_url_service_->GetTemplateURLForKeyword(keyword);
  return (template_url || host.empty())
             ? template_url
             : template_url_service_->GetTemplateURLForHost(host);
}

bool SearchEngineURLStripHandler::SupportsReplacement(
    const std::u16string& keyword,
    const std::string& host,
    const SearchTermsData& search_terms_data) const {
  const TemplateURL* template_url =
      GetConstTemplateURLWithKeyword(keyword, host);
  return template_url->SupportsReplacement(search_terms_data);
}

bool SearchEngineURLStripHandler::KeepSearchTermsInURL(
    const std::u16string& keyword,
    const std::string& host,
    const GURL& url,
    const SearchTermsData& search_terms_data,
    const bool keep_search_intent_params,
    const bool normalize_search_terms,
    GURL* out_url,
    std::u16string* out_search_terms) const {
  const TemplateURL* template_url =
      GetConstTemplateURLWithKeyword(keyword, host);
  return template_url->KeepSearchTermsInURL(url, search_terms_data,
                                            keep_search_intent_params,
                                            normalize_search_terms, out_url);
}
