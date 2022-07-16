// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/removed_results_ranker.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {

// static
bool RemovedResultsRanker::ShouldDelegateToResult(ProviderType provider) {
  return provider == ash::AppListSearchResultType::kOmnibox;
}

RemovedResultsRanker::RemovedResultsRanker(Profile* profile) {
  base::FilePath path =
      RankerStateDirectory(profile).AppendASCII("removed_results_ranker.pb");
  proto_.Init(path, write_delay_, base::DoNothing(), base::DoNothing());
}

RemovedResultsRanker::~RemovedResultsRanker() = default;

void RemovedResultsRanker::Rank(ResultsMap& results,
                                CategoriesMap& categories,
                                ProviderType provider) {
  // Results with delegated removal are handled in
  // SearchController::InvokeResultAction().
  if (!proto_.initialized() || ShouldDelegateToResult(provider))
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
  if (!proto_.initialized())
    return;

  // Record the string ID of |result| to the storage proto's map.
  // Note: We are using a map for its set capabilities; the map value is
  // arbitrary.
  (*proto_->mutable_removed_ids())[result->id()] = false;
  proto_.StartWrite();
}

}  // namespace app_list
