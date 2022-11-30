// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_local_storage_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockLocalStorageHelper::MockLocalStorageHelper(content::BrowserContext* context)
    : browsing_data::LocalStorageHelper(context) {}

MockLocalStorageHelper::~MockLocalStorageHelper() = default;

void MockLocalStorageHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockLocalStorageHelper::DeleteStorageKey(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  ASSERT_TRUE(base::Contains(storage_keys_, storage_key));
  last_deleted_storage_key_ = storage_key;
  storage_keys_[storage_key] = false;
  std::move(callback).Run();
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
  storage_keys_[storage_key] = true;
}

void MockLocalStorageHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockLocalStorageHelper::Reset() {
  for (auto& pair : storage_keys_)
    pair.second = true;
}

bool MockLocalStorageHelper::AllDeleted() {
  for (const auto& pair : storage_keys_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
