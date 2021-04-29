// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_appcache_helper.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockAppCacheHelper::MockAppCacheHelper(content::BrowserContext* browser_context)
    : AppCacheHelper(
          browser_context->GetDefaultStoragePartition()->GetAppCacheService()) {
}

MockAppCacheHelper::~MockAppCacheHelper() {}

void MockAppCacheHelper::StartFetching(FetchCallback completion_callback) {
  ASSERT_FALSE(completion_callback.is_null());
  ASSERT_TRUE(completion_callback_.is_null());
  completion_callback_ = std::move(completion_callback);
}

void MockAppCacheHelper::DeleteAppCaches(const url::Origin& origin) {}

void MockAppCacheHelper::AddAppCacheSamples() {
  response_.emplace_back(url::Origin::Create(GURL("http://hello/")), 6,
                         base::Time());
}

void MockAppCacheHelper::Notify() {
  std::move(completion_callback_).Run(response_);
}

}  // namespace browsing_data
