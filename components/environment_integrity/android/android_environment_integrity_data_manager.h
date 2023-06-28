// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_DATA_MANAGER_H_
#define COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_DATA_MANAGER_H_

#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "components/environment_integrity/android/android_environment_integrity_data_storage.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_user_data.h"
#include "url/origin.h"

namespace environment_integrity {

// Class for storing and accessing Environment Integrity data.
class AndroidEnvironmentIntegrityDataManager
    : public content::StoragePartitionUserData<
          AndroidEnvironmentIntegrityDataManager> {
 public:
  using GetHandleCallback = base::OnceCallback<void(absl::optional<int64_t>)>;

  ~AndroidEnvironmentIntegrityDataManager() override;
  AndroidEnvironmentIntegrityDataManager(
      const AndroidEnvironmentIntegrityDataManager& other) = delete;
  AndroidEnvironmentIntegrityDataManager& operator=(
      const AndroidEnvironmentIntegrityDataManager& other) = delete;

  void GetHandle(const url::Origin& origin, GetHandleCallback callback);

  void SetHandle(const url::Origin& origin, int64_t handle);

 private:
  explicit AndroidEnvironmentIntegrityDataManager(
      content::StoragePartition* storage_partition);

  base::SequenceBound<AndroidEnvironmentIntegrityDataStorage> storage_;

  friend class content::StoragePartitionUserData<
      AndroidEnvironmentIntegrityDataManager>;

  STORAGE_PARTITION_USER_DATA_KEY_DECL();
};

}  // namespace environment_integrity

#endif  // COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_DATA_MANAGER_H_
