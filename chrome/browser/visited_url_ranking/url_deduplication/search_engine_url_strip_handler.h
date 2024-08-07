// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITED_URL_RANKING_URL_DEDUPLICATION_SEARCH_ENGINE_URL_STRIP_HANDLER_H_
#define CHROME_BROWSER_VISITED_URL_RANKING_URL_DEDUPLICATION_SEARCH_ENGINE_URL_STRIP_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_deduplication/url_strip_handler.h"

class GURL;
class TemplateURL;

namespace url_deduplication {

class SearchEngineURLStripHandler : public URLStripHandler {
 public:
  explicit SearchEngineURLStripHandler(TemplateURLService* template_url_service,
                                       bool keep_search_intent_params = true,
                                       bool normalize_search_terms = true,
                                       std::u16string keyword = u"");

  SearchEngineURLStripHandler(const SearchEngineURLStripHandler&) = delete;
  SearchEngineURLStripHandler& operator=(const SearchEngineURLStripHandler&) =
      delete;

  ~SearchEngineURLStripHandler() override = default;

  // URLStripHandler:
  GURL StripExtraParams(GURL) override;

 private:
  const TemplateURL* GetConstTemplateURLWithKeyword(
      const std::u16string& keyword,
      const std::string& host) const;

  const raw_ptr<TemplateURLService> template_url_service_;
  bool keep_search_intent_params_;
  bool normalize_search_terms_;
  std::u16string keyword_;
};

}  // namespace url_deduplication

#endif  // CHROME_BROWSER_VISITED_URL_RANKING_URL_DEDUPLICATION_SEARCH_ENGINE_URL_STRIP_HANDLER_H_
