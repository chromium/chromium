// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

#include "base/notreached.h"

namespace app_list {

void Ranker::Start(const std::u16string& query,
                   ResultsMap& results,
                   CategoriesList& categories) {}

absl::optional<std::vector<double>> Ranker::RankResults(
    ResultsMap& results,
    CategoriesList& categories,
    ProviderType provider) {
  // TODO(crbug.com/1199206): Add a NOTREACHED here once all rankers have been
  // appropriately split in category or result rankers.
  return absl::nullopt;
}

absl::optional<std::vector<double>> Ranker::RankCategories(
    ResultsMap& results,
    CategoriesList& categories,
    ProviderType provider) {
  // TODO(crbug.com/1199206): Add a NOTREACHED here once all rankers have been
  // appropriately split in category or result rankers.
  return absl::nullopt;
}

void Ranker::Train(const LaunchData& launch) {}

void Ranker::Remove(ChromeSearchResult* result) {}

}  // namespace app_list
