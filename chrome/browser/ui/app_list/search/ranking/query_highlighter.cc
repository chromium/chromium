// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/query_highlighter.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"

namespace app_list {

QueryHighlighter::QueryHighlighter() = default;
QueryHighlighter::~QueryHighlighter() = default;

void QueryHighlighter::Start(const std::u16string& query,
                             ResultsMap& results,
                             CategoriesList& categories) {
  last_query_ = query;
}

void QueryHighlighter::UpdateResultRanks(ResultsMap& results,
                                         ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    // Don't perform query highlighting on answer cards.
    if (result->display_type() == ChromeSearchResult::DisplayType::kAnswerCard)
      continue;

    result->SetTitleTags(CalculateTags(last_query_, result->title()));
    result->SetDetailsTags(CalculateTags(last_query_, result->details()));
  }
}

}  // namespace app_list
