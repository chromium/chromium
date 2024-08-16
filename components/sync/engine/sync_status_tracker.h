// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_STATUS_TRACKER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_STATUS_TRACKER_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/sync_engine_event_listener.h"
#include "components/sync/engine/sync_status.h"

namespace sync_pb {
class NigoriSpecifics_TrustedVaultDebugInfo;
}  // namespace sync_pb

namespace syncer {

struct SyncCycleEvent;

// This class watches various sync engine components, updating its internal
// state upon change and firing the callback injected on construction.
//
// Most of this data ends up on the chrome://sync-internals page. But the page
// is only 'pinged' to update itself at the end of a sync cycle. A user could
// refresh manually, but unless their timing is excellent it's unlikely that a
// user will see any state in mid-sync cycle. We have no plans to change this.
// However, we will continue to collect data and update state mid-sync-cycle in
// case we need to debug slow or stuck sync cycles.
class SyncStatusTracker : public SyncEngineEventListener {
 public:
  explicit SyncStatusTracker(
      const base::RepeatingCallback<void(const SyncStatus&)>&
          status_changed_callback);
  ~SyncStatusTracker() override;

  SyncStatusTracker(const SyncStatusTracker&) = delete;
  SyncStatusTracker& operator=(const SyncStatusTracker&) = delete;

  // SyncEngineEventListener implementation.
  void OnSyncCycleEvent(const SyncCycleEvent& event) override;
  void OnActionableProtocolError(const SyncProtocolError& error) override;
  void OnRetryTimeChanged(base::Time retry_time) override;
  void OnThrottledTypesChanged(DataTypeSet throttled_types) override;
  void OnBackedOffTypesChanged(DataTypeSet backed_off_types) override;
  void OnMigrationRequested(DataTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  void SetNotificationsEnabled(bool notifications_enabled);

  void IncrementNotificationsReceived();

  void SetEncryptedTypes(DataTypeSet types);
  void SetCryptographerCanEncrypt(bool can_encrypt);
  void SetCryptoHasPendingKeys(bool has_pending_keys);
  void SetPassphraseType(PassphraseType type);
  void SetHasKeystoreKey(bool has_keystore_key);
  void SetKeystoreMigrationTime(const base::Time& migration_time);
  void SetTrustedVaultDebugInfo(
      const sync_pb::NigoriSpecifics_TrustedVaultDebugInfo&
          trusted_vault_debug_info);

  void SetCacheGuid(const std::string& cache_guid);
  void SetHasPendingInvalidations(DataType type,
                                  bool has_pending_invalidations);

  void SetLocalBackendFolder(const std::string& folder);

 protected:
  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  SyncStatus CalcSyncing(const SyncCycleEvent& event) const;
  SyncStatus CreateBlankStatus() const;

  SyncStatus status_;

 private:
  const base::RepeatingCallback<void(const SyncStatus&)>
      status_changed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_STATUS_TRACKER_H_
