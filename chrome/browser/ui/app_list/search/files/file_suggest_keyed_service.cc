// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"

namespace app_list {

FileSuggestKeyedService::FileSuggestKeyedService(Profile* profile)
    : item_suggest_cache_(std::make_unique<ItemSuggestCache>(
          profile,
          profile->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess())) {}

FileSuggestKeyedService::~FileSuggestKeyedService() = default;

absl::optional<ItemSuggestCache::Results>
FileSuggestKeyedService::GetSuggestData(SuggestDataType type) {
  switch (type) {
    case SuggestDataType::kItemSuggest:
      return item_suggest_cache_->GetResults();
  }

  NOTREACHED();
  return absl::nullopt;
}

void FileSuggestKeyedService::MaybeUpdateItemSuggestCache() {
  item_suggest_cache_->MaybeUpdateCache();
}

base::CallbackListSubscription
FileSuggestKeyedService ::RegisterItemSuggestUpdateCallback(
    ItemSuggestCache::OnResultsCallback callback) {
  return item_suggest_cache_->RegisterCallback(callback);
}

}  // namespace app_list
