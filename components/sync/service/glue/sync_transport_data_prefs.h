// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_GLUE_SYNC_TRANSPORT_DATA_PREFS_H_
#define COMPONENTS_SYNC_SERVICE_GLUE_SYNC_TRANSPORT_DATA_PREFS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/base/gaia_id_hash.h"

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
  SyncTransportDataPrefs(PrefService* pref_service,
                         const signin::GaiaIdHash& gaia_id_hash);
  SyncTransportDataPrefs(const SyncTransportDataPrefs&) = delete;
  SyncTransportDataPrefs& operator=(const SyncTransportDataPrefs&) = delete;
  ~SyncTransportDataPrefs();

  // Clears all account-keyed preferences for the current account (i.e. the one
  // passed into the constructor).
  void ClearForCurrentAccount();

  // Clears all account-keyed preferences for all accounts that are NOT in
  // `available_gaia_ids`.
  static void KeepAccountSettingsPrefsOnlyForUsers(
      PrefService* pref_service,
      const std::vector<signin::GaiaIdHash>& available_gaia_ids);

  // The Gaia ID for which the sync machinery was last active, and for which
  // there may be data around. Cleared when sync gets disabled (typically on
  // signout) and data was removed.
  void SetCurrentSyncingGaiaId(const std::string& gaia_id);
  std::string GetCurrentSyncingGaiaId() const;
  void ClearCurrentSyncingGaiaId();
  static bool HasCurrentSyncingGaiaId(const PrefService* pref_service);
  static void ClearCurrentSyncingGaiaId(PrefService* pref_service);

  // All of the following prefs are Gaia-keyed (to the `gaia_id_hash`) passed
  // to the constructor):

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

 private:
  // Never null.
  const raw_ptr<PrefService> pref_service_;

  const signin::GaiaIdHash gaia_id_hash_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_GLUE_SYNC_TRANSPORT_DATA_PREFS_H_
