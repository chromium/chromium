// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results.pb.h"
#include "chrome/browser/ui/app_list/search/util/persistent_proto.h"

namespace app_list {

// A ranker which removes results which have previously been marked for removal
// from the launcher search results list.
//
// On a call to Remove(), the result slated for removal is recorded, and queued
// to be persisted to disk.
// On a call to Rank(), previously removed results are filtered out.
class RemovedResultsRanker : public Ranker {
 public:
  explicit RemovedResultsRanker(PersistentProto<RemovedResultsProto> proto);
  ~RemovedResultsRanker() override;

  RemovedResultsRanker(const RemovedResultsRanker&) = delete;
  RemovedResultsRanker& operator=(const RemovedResultsRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

  void Remove(ChromeSearchResult* result) override;

 private:
  friend class RemovedResultsRankerTest;

  // Whether the ranker has finished reading from disk.
  bool initialized() const { return proto_.initialized(); }

  // How long to wait until writing any |proto_| updates to disk.
  base::TimeDelta write_delay_;

  PersistentProto<RemovedResultsProto> proto_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_
