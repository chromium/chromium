// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"

namespace app_list {

RankerDelegate::RankerDelegate(Profile* profile,
                               SearchController* controller) {}

RankerDelegate::~RankerDelegate() {}

void RankerDelegate::Start(const std::u16string& query) {
  for (auto& ranker : rankers_)
    ranker->Start(query);
}

void RankerDelegate::Rank(ResultsMap& results, ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->Rank(results, provider);
}

void RankerDelegate::Train(const LaunchData& launch) {
  for (auto& ranker : rankers_)
    ranker->Train(launch);
}

void RankerDelegate::AddRanker(std::unique_ptr<Ranker> ranker) {
  rankers_.emplace_back(std::move(ranker));
}

}  // namespace app_list
