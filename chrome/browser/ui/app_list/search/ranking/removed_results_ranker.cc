// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/removed_results_ranker.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {

RemovedResultsRanker::RemovedResultsRanker(
    PersistentProto<RemovedResultsProto> proto)
    : proto_(std::move(proto)) {
  proto_.Init();
}

RemovedResultsRanker::~RemovedResultsRanker() = default;

void RemovedResultsRanker::UpdateResultRanks(ResultsMap& results,
                                             ProviderType provider) {
  if (!initialized())
    return;

  const auto it = results.find(provider);
  DCHECK(it != results.end());

  // Filter any results whose IDs have been recorded as for removal.
  for (const auto& result : it->second) {
    if (proto_->removed_ids().contains(result->id())) {
      result->scoring().filter = true;
    }
  }
}

void RemovedResultsRanker::Remove(ChromeSearchResult* result) {
  if (!initialized())
    return;

  // Record the string ID of |result| to the storage proto's map.
  // Note: We are using a map for its set capabilities; the map value is
  // arbitrary.
  (*proto_->mutable_removed_ids())[result->id()] = false;
  proto_.StartWrite();
}

}  // namespace app_list
