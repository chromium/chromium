// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_H_
#define COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_device_salt/media_device_salt_database.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

class PrefService;

namespace media_device_salt {

BASE_DECLARE_FEATURE(kMediaDeviceIdPartitioning);
BASE_DECLARE_FEATURE(kMediaDeviceIdRandomSaltsPerStorageKey);

class MediaDeviceIDSalt;

// Service that manages salts used to generate media device IDs.
class MediaDeviceSaltService : public KeyedService {
 public:
  explicit MediaDeviceSaltService(PrefService* pref_service,
                                  const base::FilePath& path);
  ~MediaDeviceSaltService() override;

  MediaDeviceSaltService(const MediaDeviceSaltService&) = delete;
  MediaDeviceSaltService& operator=(const MediaDeviceSaltService&) = delete;

  // Returns the salt for the given `storage_ket` via `callback`.
  void GetSalt(const blink::StorageKey& storage_key,
               base::OnceCallback<void(const std::string&)> callback);

  // Deletes salts in the given time range whose storage keys match the given
  // `matcher`. If `matcher` is null, all entries in the given time range are
  // deleted. `done_closure` is invoked after the operation is complete.
  void DeleteSalts(base::Time delete_begin,
                   base::Time delete_end,
                   content::StoragePartition::StorageKeyMatcherFunction matcher,
                   base::OnceClosure done_closure);

  // Deletes the salt for the given `storage_key`. `done_closure` is invoked
  // after the operation is complete.
  void DeleteSalt(const blink::StorageKey& storage_key,
                  base::OnceClosure done_closure);

  // Returns all the storage keys that have an associated salt (via `callback`).
  void GetAllStorageKeys(
      base::OnceCallback<void(std::vector<blink::StorageKey>)> callback);

 private:
  void FinalizeGetSalt(base::OnceCallback<void(const std::string&)> callback,
                       std::optional<std::string> new_salt);
  void FinalizeDeleteSalts(base::OnceClosure done_closure);
  void FinalizeGetAllStorageKeys(
      base::OnceCallback<void(std::vector<blink::StorageKey>)> callback,
      std::vector<blink::StorageKey> storage_keys);

  // TODO(crbug.com/40922096): Remove these operations.
  std::string GetGlobalSalt();
  void ResetGlobalSalt();

  // Ephemeral salt for opaque origins or if the database is broken.
  std::string fallback_salt_;
  base::Time fallback_salt_creation_time_;

  // TODO(crbug.com/40922096): Remove these two fields.
  const scoped_refptr<MediaDeviceIDSalt> media_device_id_salt_;
  const raw_ptr<PrefService> pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::SequenceBound<MediaDeviceSaltDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<MediaDeviceSaltService> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace media_device_salt

#endif  // COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_H_
