// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_TRANSPORT_DATA_PREFS_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_TRANSPORT_DATA_PREFS_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/prefs/pref_member.h"
#include "components/sync/base/model_type.h"

class PrefRegistrySimple;
class PrefService;

namespace syncer {

// Thin wrapper for "bookkeeping" sync preferences, such as the last synced
// time, whether the last shutdown was clean, etc. Does *NOT* include sync
// preferences which are directly user-controlled, such as the set of selected
// types.
//
// In order to use this class RegisterProfilePrefs() needs to be invoked first.
class SyncTransportDataPrefs {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // |pref_service| must not be null and must outlive this object.
  explicit SyncTransportDataPrefs(PrefService* pref_service);
  SyncTransportDataPrefs(const SyncTransportDataPrefs&) = delete;
  SyncTransportDataPrefs& operator=(const SyncTransportDataPrefs&) = delete;
  ~SyncTransportDataPrefs();

  // Clears all preferences in this class.
  void ClearAll();

  void SetGaiaId(const std::string& gaia_id);
  std::string GetGaiaId() const;
  void SetCacheGuid(const std::string& cache_guid);
  std::string GetCacheGuid() const;
  void SetBirthday(const std::string& birthday);
  std::string GetBirthday() const;
  void SetBagOfChips(const std::string& bag_of_chips);
  std::string GetBagOfChips() const;

  base::Time GetLastSyncedTime() const;
  void SetLastSyncedTime(base::Time time);

  base::Time GetLastPollTime() const;
  void SetLastPollTime(base::Time time);

  base::TimeDelta GetPollInterval() const;
  void SetPollInterval(base::TimeDelta interval);

  // Get/set for the last known sync invalidation versions.
  std::map<ModelType, int64_t> GetInvalidationVersions() const;
  void UpdateInvalidationVersions(
      const std::map<ModelType, int64_t>& invalidation_versions);

  // Migrates invalidation versions from a deprecated pref to the current one.
  // Does nothing if the pref was already migrated. Should be called during
  // browser startup.
  static void MigrateInvalidationVersions(PrefService* pref_service);

 private:
  // Never null.
  const raw_ptr<PrefService> pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

void ClearObsoleteKeystoreBootstrapTokenPref(PrefService* pref_service);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_TRANSPORT_DATA_PREFS_H_
