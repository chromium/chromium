// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_data_manager.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/storage_partition_config.h"

namespace {

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("EnvironmentIntegrity");

base::FilePath GetDBPath(content::StoragePartition* storage_partition) {
  if (storage_partition->GetConfig().in_memory()) {
    // AndroidEnvironmentIntegrityDataStorage expects an empty file path in
    // order to open the DB in memory.
    return base::FilePath();
  }

  return storage_partition->GetPath().Append(kDatabasePath);
}

}  // namespace

namespace environment_integrity {

STORAGE_PARTITION_USER_DATA_KEY_IMPL(AndroidEnvironmentIntegrityDataManager);

AndroidEnvironmentIntegrityDataManager::AndroidEnvironmentIntegrityDataManager(
    content::StoragePartition* storage_partition)
    : content::StoragePartitionUserData<AndroidEnvironmentIntegrityDataManager>(
          storage_partition),
      storage_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                    base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
               GetDBPath(storage_partition)) {
  storage_partition->AddObserver(this);
}

AndroidEnvironmentIntegrityDataManager::
    ~AndroidEnvironmentIntegrityDataManager() = default;

void AndroidEnvironmentIntegrityDataManager::GetHandle(
    const url::Origin& origin,
    GetHandleCallback callback) {
  storage_.AsyncCall(&AndroidEnvironmentIntegrityDataStorage::GetHandle)
      .WithArgs(origin)
      .Then(std::move(callback));
}

void AndroidEnvironmentIntegrityDataManager::SetHandle(
    const url::Origin& origin,
    int64_t handle) {
  storage_.AsyncCall(&AndroidEnvironmentIntegrityDataStorage::SetHandle)
      .WithArgs(origin, handle);
}

void AndroidEnvironmentIntegrityDataManager::OnStorageKeyDataCleared(
    uint32_t remove_mask,
    content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    const base::Time begin,
    const base::Time end) {
  if (remove_mask &
      content::StoragePartition::REMOVE_DATA_MASK_ENVIRONMENT_INTEGRITY) {
    storage_.AsyncCall(&AndroidEnvironmentIntegrityDataStorage::ClearData)
        .WithArgs(std::move(storage_key_matcher));
  }
}

}  // namespace environment_integrity
