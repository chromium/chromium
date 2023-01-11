// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_indexed_db_helper.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace browsing_data {

MockIndexedDBHelper::MockIndexedDBHelper(
    content::BrowserContext* browser_context)
    : IndexedDBHelper(browser_context->GetDefaultStoragePartition()) {}

MockIndexedDBHelper::~MockIndexedDBHelper() {}

void MockIndexedDBHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockIndexedDBHelper::DeleteIndexedDB(
    const blink::StorageKey& storage_key,
    base::OnceCallback<void(bool)> callback) {
  ASSERT_TRUE(base::Contains(storage_keys_, storage_key));
  storage_keys_[storage_key] = false;

  bool success = true;
  std::move(callback).Run(success);
}

void MockIndexedDBHelper::AddIndexedDBSamples() {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://idbhost1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://idbhost2:2/");

  content::StorageUsageInfo info1(kStorageKey1, 1, base::Time());
  response_.push_back(info1);
  storage_keys_[kStorageKey1] = true;

  content::StorageUsageInfo info2(kStorageKey2, 2, base::Time());
  response_.push_back(info2);
  storage_keys_[kStorageKey2] = true;
}

void MockIndexedDBHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockIndexedDBHelper::Reset() {
  for (auto& pair : storage_keys_)
    pair.second = true;
}

bool MockIndexedDBHelper::AllDeleted() {
  for (const auto& pair : storage_keys_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
