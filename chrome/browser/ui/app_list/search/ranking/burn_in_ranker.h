// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_BURN_IN_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_BURN_IN_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

namespace app_list {

// A ranker which implements a "burn-in" period for the updating of results.
//
// This ranker has two main tasks:
//
// 1) Delay the release of results until after the burn-in period has ended.
// The purpose of this is to prevent the rapid and jittery updating of the
// results list on initial display.
//
// 2) Mark each result as having arrived pre- or post-burn-in time.
// Post-burn-in results may undergo special handling (e.g. append-only),
// with exact details TBD.
//
// TODO(crbug.com/1279686): Update description when implemented.
class BurnInRanker : public Ranker {
 public:
  BurnInRanker() = default;
  ~BurnInRanker() override = default;

  BurnInRanker(const BurnInRanker&) = delete;
  BurnInRanker& operator=(const BurnInRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_BURN_IN_RANKER_H_
