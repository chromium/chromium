// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/system/system_monitor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/media_device_salt/media_device_id_salt.h"
#include "components/media_device_salt/media_device_salt_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace media_device_salt {

BASE_FEATURE(kMediaDeviceIdPartitioning,
             "MediaDeviceIdPartitioning",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kMediaDeviceIdRandomSaltsPerStorageKey,
             "MediaDeviceIdRandomSaltsPerStorageKey",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateDatabaseTaskRunner() {
  // We use a SequencedTaskRunner so that there is a global ordering to a
  // storage key's directory operations.
  return base::ThreadPool::CreateSequencedTaskRunner({
      base::MayBlock(),  // For File I/O
      base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::BLOCK_SHUTDOWN,  // To allow clean shutdown
  });
}

}  // namespace

MediaDeviceSaltService::MediaDeviceSaltService(PrefService* pref_service,
                                               const base::FilePath& path)
    : fallback_salt_(CreateRandomSalt()),
      fallback_salt_creation_time_(base::Time::Now()),
      media_device_id_salt_(
          base::MakeRefCounted<MediaDeviceIDSalt>(pref_service)),
      pref_service_(pref_service),
      db_(base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)
              ? base::SequenceBound<MediaDeviceSaltDatabase>(
                    CreateDatabaseTaskRunner(),
                    path)
              : base::SequenceBound<MediaDeviceSaltDatabase>()) {}

MediaDeviceSaltService::~MediaDeviceSaltService() = default;

void MediaDeviceSaltService::GetSalt(
    const blink::StorageKey& storage_key,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)) {
    std::move(callback).Run(GetGlobalSalt());
    return;
  }

  if (storage_key.origin().opaque()) {
    std::move(callback).Run(fallback_salt_);
    return;
  }

  std::optional<std::string> candidate_salt;
  if (!base::FeatureList::IsEnabled(kMediaDeviceIdRandomSaltsPerStorageKey)) {
    candidate_salt = GetGlobalSalt();
  }

  db_.AsyncCall(&MediaDeviceSaltDatabase::GetOrInsertSalt)
      .WithArgs(storage_key, candidate_salt)
      .Then(base::BindOnce(&MediaDeviceSaltService::FinalizeGetSalt,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaDeviceSaltService::FinalizeGetSalt(
    base::OnceCallback<void(const std::string&)> callback,
    std::optional<std::string> salt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(salt.has_value() ? *salt : fallback_salt_);
}

void MediaDeviceSaltService::DeleteSalts(
    base::Time delete_begin,
    base::Time delete_end,
    content::StoragePartition::StorageKeyMatcherFunction matcher,
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (matcher) {
    if (!base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)) {
      std::move(done_closure).Run();
      return;
    }
  } else {
    if (!base::FeatureList::IsEnabled(kMediaDeviceIdRandomSaltsPerStorageKey) ||
        !base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)) {
      ResetGlobalSalt();
    }
    if (!base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)) {
      FinalizeDeleteSalts(std::move(done_closure));
      return;
    }

    // Reset the fallback key if the deletion period includes its creation time.
    if (delete_begin <= fallback_salt_creation_time_ &&
        fallback_salt_creation_time_ <= delete_end) {
      fallback_salt_ = CreateRandomSalt();
      fallback_salt_creation_time_ = base::Time::Now();
    }
  }

  db_.AsyncCall(&MediaDeviceSaltDatabase::DeleteEntries)
      .WithArgs(delete_begin, delete_end, std::move(matcher))
      .Then(base::BindOnce(&MediaDeviceSaltService::FinalizeDeleteSalts,
                           weak_factory_.GetWeakPtr(),
                           std::move(done_closure)));
}

void MediaDeviceSaltService::FinalizeDeleteSalts(
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Propagate device change notifications, for anything currently using devices
  // which will now have new IDs.
  if (base::SystemMonitor* monitor = base::SystemMonitor::Get()) {
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  }
  std::move(done_closure).Run();
}

void MediaDeviceSaltService::DeleteSalt(const blink::StorageKey& storage_key,
                                        base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)) {
    std::move(done_closure).Run();
    return;
  }
  db_.AsyncCall(&MediaDeviceSaltDatabase::DeleteEntry)
      .WithArgs(storage_key)
      .Then(base::BindOnce(&MediaDeviceSaltService::FinalizeDeleteSalts,
                           weak_factory_.GetWeakPtr(),
                           std::move(done_closure)));
}

void MediaDeviceSaltService::GetAllStorageKeys(
    base::OnceCallback<void(std::vector<blink::StorageKey>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(kMediaDeviceIdPartitioning)) {
    std::move(callback).Run({});
    return;
  }
  db_.AsyncCall(&MediaDeviceSaltDatabase::GetAllStorageKeys)
      .Then(base::BindOnce(&MediaDeviceSaltService::FinalizeGetAllStorageKeys,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaDeviceSaltService::FinalizeGetAllStorageKeys(
    base::OnceCallback<void(std::vector<blink::StorageKey>)> callback,
    std::vector<blink::StorageKey> storage_keys) {
  std::move(callback).Run(std::move(storage_keys));
}

std::string MediaDeviceSaltService::GetGlobalSalt() {
  return media_device_id_salt_->GetSalt();
}

void MediaDeviceSaltService::ResetGlobalSalt() {
  MediaDeviceIDSalt::Reset(pref_service_);
}

}  // namespace media_device_salt
