// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/answer_ranker.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

AnswerRanker::AnswerRanker() = default;
AnswerRanker::~AnswerRanker() = default;

void AnswerRanker::UpdateResultRanks(ResultsMap& results,
                                     ProviderType provider) {
  if (provider != ProviderType::kOmnibox)
    return;

  const auto it = results.find(provider);
  DCHECK(it != results.end());

  ChromeSearchResult* top_answer = nullptr;
  double top_score;
  for (const auto& result : it->second) {
    if (result->display_type() != ash::SearchResultDisplayType::kAnswerCard)
      continue;

    // Compare this result to the existing answer, if any, and hide the one with
    // lower score.
    // TODO(crbug.com/1275408): If we ever expect there to be more than one
    // answer candidate, then it should instead be properly demoted into a list
    // result.
    const double score = result->relevance();
    if (!top_answer) {
      top_answer = result.get();
      top_score = score;
    } else if (score > top_score) {
      top_answer->scoring().filter = true;
      top_answer = result.get();
      top_score = score;
    } else {
      result->scoring().filter = true;
    }
  }
}

}  // namespace app_list
