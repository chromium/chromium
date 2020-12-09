// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_indexed_db_helper.h"

#include "base/callback.h"
#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browsing_data {

MockIndexedDBHelper::MockIndexedDBHelper(
    content::BrowserContext* browser_context)
    : IndexedDBHelper(content::BrowserContext::GetDefaultStoragePartition(
          browser_context)) {}

MockIndexedDBHelper::~MockIndexedDBHelper() {}

void MockIndexedDBHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockIndexedDBHelper::DeleteIndexedDB(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  ASSERT_TRUE(base::Contains(origins_, origin));
  origins_[origin] = false;

  bool success = true;
  std::move(callback).Run(success);
}

void MockIndexedDBHelper::AddIndexedDBSamples() {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://idbhost1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://idbhost2:2/"));

  content::StorageUsageInfo info1(kOrigin1, 1, base::Time());
  response_.push_back(info1);
  origins_[kOrigin1] = true;

  content::StorageUsageInfo info2(kOrigin2, 2, base::Time());
  response_.push_back(info2);
  origins_[kOrigin2] = true;
}

void MockIndexedDBHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockIndexedDBHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockIndexedDBHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
