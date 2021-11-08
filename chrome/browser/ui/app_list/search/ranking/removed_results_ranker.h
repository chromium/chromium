// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/ranking/persistent_proto.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results.pb.h"

namespace app_list {

// A ranker which removes results which have previously been marked for removal
// from the launcher search results list.
//
// On a call to Remove(), the result slated for removal is recorded, and queued
// to be persisted to disk.
// On a call to Rank(), previously removed results are filtered out.
class RemovedResultsRanker : public Ranker {
 public:
  explicit RemovedResultsRanker(Profile* profile);
  ~RemovedResultsRanker() override;

  RemovedResultsRanker(const RemovedResultsRanker&) = delete;
  RemovedResultsRanker& operator=(const RemovedResultsRanker&) = delete;

  // Ranker:
  void Rank(ResultsMap& results,
            CategoriesMap& categories,
            ProviderType provider) override;

  void Remove(ChromeSearchResult* result) override;

  // Returns whether result removal requests for results from type |provider|
  // should be delegated to the result, as opposed to handled by this class.
  // Currently this returns true in one case:
  //   1) Omnibox results, whose removal requests are handled by the omnibox
  //      autocomplete controller. The Omnibox is unique amongst our search
  //      providers in that it has a backend which supports result removal.
  static bool ShouldDelegateToResult(ProviderType provider);

 private:
  // How long to wait until writing any |proto_| updates to disk.
  base::TimeDelta write_delay_ = base::Seconds(30);

  PersistentProto<RemovedResultsProto> proto_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_
