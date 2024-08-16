// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/url_deduplication/search_engine_url_strip_handler.h"

#include <string>

#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace url_deduplication {

SearchEngineURLStripHandler::SearchEngineURLStripHandler(
    TemplateURLService* template_url_service,
    bool keep_search_intent_params,
    bool normalize_search_terms,
    std::u16string keyword)
    : template_url_service_(template_url_service),
      keep_search_intent_params_(keep_search_intent_params),
      normalize_search_terms_(normalize_search_terms),
      keyword_(keyword) {}

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
    const TemplateURL* template_url = GetConstTemplateURLWithKeyword(
        keyword_, stripped_destination_url.host());
    if (template_url && template_url->SupportsReplacement(
                            template_url_service_->search_terms_data())) {
      template_url->KeepSearchTermsInURL(
          url, template_url_service_->search_terms_data(),
          keep_search_intent_params_, normalize_search_terms_,
          &stripped_destination_url);
    }
  }

  return stripped_destination_url;
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

}  // namespace url_deduplication
