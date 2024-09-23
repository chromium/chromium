// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_local_storage_helper.h"

#include "base/functional/callback.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockLocalStorageHelper::MockLocalStorageHelper(
    content::StoragePartition* storage_partition)
    : browsing_data::LocalStorageHelper(storage_partition) {}

MockLocalStorageHelper::~MockLocalStorageHelper() = default;

void MockLocalStorageHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockLocalStorageHelper::AddLocalStorageSamples() {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:2/");
  AddLocalStorageForStorageKey(kStorageKey1, 1);
  AddLocalStorageForStorageKey(kStorageKey2, 2);
}

void MockLocalStorageHelper::AddLocalStorageForStorageKey(
    const blink::StorageKey& storage_key,
    int64_t size) {
  response_.emplace_back(storage_key, size, base::Time());
}

void MockLocalStorageHelper::Notify() {
  std::move(callback_).Run(response_);
}

}  // namespace browsing_data
