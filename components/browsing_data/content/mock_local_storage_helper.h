// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_LOCAL_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_LOCAL_STORAGE_HELPER_H_

#include <list>

#include "base/functional/callback.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class StoragePartition;
}  // namespace content

namespace browsing_data {

// Mock for browsing_data::LocalStorageHelper.
// Use AddLocalStorageSamples() or add directly to response_ list, then
// call Notify().
class MockLocalStorageHelper : public browsing_data::LocalStorageHelper {
 public:
  explicit MockLocalStorageHelper(content::StoragePartition* storage_partition);

  MockLocalStorageHelper(const MockLocalStorageHelper&) = delete;
  MockLocalStorageHelper& operator=(const MockLocalStorageHelper&) = delete;

  // browsing_data::LocalStorageHelper implementation.
  void StartFetching(FetchCallback callback) override;

  // Adds some LocalStorageInfo samples.
  void AddLocalStorageSamples();

  // Add a LocalStorageInfo entry for a single `storage_key`.
  void AddLocalStorageForStorageKey(const blink::StorageKey& storage_key,
                                    int64_t size);

  // Notifies the callback.
  void Notify();

 private:
  ~MockLocalStorageHelper() override;

  FetchCallback callback_;

  std::list<content::StorageUsageInfo> response_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_LOCAL_STORAGE_HELPER_H_
