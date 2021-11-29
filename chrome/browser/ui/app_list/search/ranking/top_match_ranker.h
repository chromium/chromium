// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TOP_MATCH_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TOP_MATCH_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

namespace app_list {

// A ranker that inspects the scores of incoming results and picks out some of
// them to become 'top matches' to be moved to the top of the results list.
//
// Choosing top matches is done by simply thresholding the scores of incoming
// results. It is assumed that this runs after scores have been transformed to a
// near-uniform distribution per-provider by the ScoreNormalizingRanker, and so
// a score of 0.95 indicates a 95th percentile result from a provider, for
// example. It is also assumed that this runs before the category ranker, which
// moves the scores out of the normalized range [0,1].
class TopMatchRanker : public Ranker {
 public:
  TopMatchRanker();
  ~TopMatchRanker() override;

  TopMatchRanker(const TopMatchRanker&) = delete;
  TopMatchRanker& operator=(const TopMatchRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TOP_MATCH_RANKER_H_
