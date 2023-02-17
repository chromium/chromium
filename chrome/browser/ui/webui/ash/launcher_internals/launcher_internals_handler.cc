// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/types_util.h"

namespace ash {

LauncherInternalsHandler::LauncherInternalsHandler(
    app_list::SearchController* search_controller,
    mojo::PendingRemote<launcher_internals::mojom::Page> page)
    : page_(std::move(page)) {
  DCHECK(search_controller);
  search_controller_observation_.Observe(search_controller);
}

LauncherInternalsHandler::~LauncherInternalsHandler() = default;

void LauncherInternalsHandler::OnResultsAdded(
    const std::u16string& query,
    const std::vector<app_list::KeywordInfo>& extracted_keyword_info,
    const std::vector<const ChromeSearchResult*>& results) {
  std::vector<launcher_internals::mojom::ResultPtr> internals_results;
  std::vector<std::string> keywords;

  for (const auto& keyword : extracted_keyword_info) {
    keywords.emplace_back(base::UTF16ToUTF8(keyword.query_token));
  }

  for (const auto* result : results) {
    auto ranker_scores = result->ranker_scores();
    ranker_scores["Relevance"] = result->relevance();

    internals_results.emplace_back(launcher_internals::mojom::Result::New(
        result->id(), base::UTF16ToUTF8(result->title()),
        base::UTF16ToUTF8(result->details()),
        app_list::ResultTypeToString(result->result_type()),
        app_list::MetricsTypeToString(result->metrics_type()),
        app_list::DisplayTypeToString(result->display_type()),
        result->display_score(), ranker_scores));
  }

  page_->UpdateResults(base::UTF16ToUTF8(query), keywords,
                       std::move(internals_results));
}

}  // namespace ash
