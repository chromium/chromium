// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/query_highlighter.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"

namespace app_list {
namespace {

using TextVector = std::vector<ash::SearchResultTextItem>;

void SetMatchTags(const std::u16string& query, TextVector& text_vector) {
  for (auto& item : text_vector) {
    if (item.GetType() != ash::SearchResultTextItemType::kString)
      continue;

    ash::SearchResultTags tags = item.GetTextTags();
    // Remove any existing match tags before re-calculating them.
    for (auto& tag : tags) {
      tag.styles &= ~ash::SearchResultTag::MATCH;
    }
    AppendMatchTags(query, item.GetText(), &tags);
    item.SetTextTags(tags);
  }
}

}  // namespace

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
    TextVector title_vector = result->title_text_vector();
    SetMatchTags(last_query_, title_vector);
    result->SetTitleTextVector(title_vector);

    if (result->display_type() !=
        ChromeSearchResult::DisplayType::kAnswerCard) {
      TextVector details_vector = result->details_text_vector();
      SetMatchTags(last_query_, details_vector);
      result->SetDetailsTextVector(details_vector);
    }
  }
}

}  // namespace app_list
