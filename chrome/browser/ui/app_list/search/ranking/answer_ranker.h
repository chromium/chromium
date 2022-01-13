// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_ANSWER_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_ANSWER_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

namespace app_list {

// A ranker that selects at most one result to be an Answer card. This ranker
// also hides any Omnibox answers that are not selected.
class AnswerRanker : public Ranker {
 public:
  AnswerRanker();
  ~AnswerRanker() override;

  AnswerRanker(const AnswerRanker&) = delete;
  AnswerRanker& operator=(const AnswerRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_ANSWER_RANKER_H_
