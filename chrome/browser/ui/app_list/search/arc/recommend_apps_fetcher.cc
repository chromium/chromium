// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/recommend_apps_fetcher.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/arc/recommend_apps_fetcher_impl.h"
#include "content/public/browser/storage_partition.h"

namespace app_list {

// static
std::unique_ptr<RecommendAppsFetcher> RecommendAppsFetcher::Create(
    RecommendAppsFetcherDelegate* delegate) {
  return std::make_unique<RecommendAppsFetcherImpl>(
      delegate, content::BrowserContext::GetDefaultStoragePartition(
                    ProfileManager::GetActiveUserProfile())
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
}

}  // namespace app_list
