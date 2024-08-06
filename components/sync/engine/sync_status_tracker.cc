// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_status_tracker.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "components/sync/engine/sync_cycle_event.h"

namespace syncer {

SyncStatusTracker::SyncStatusTracker(
    const base::RepeatingCallback<void(const SyncStatus&)>&
        status_changed_callback)
    : status_changed_callback_(status_changed_callback) {
  DCHECK(status_changed_callback_);
}

SyncStatusTracker::~SyncStatusTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SyncStatus SyncStatusTracker::CreateBlankStatus() const {
  // Status is initialized with the previous status value.  Variables
  // whose values accumulate (e.g. lifetime counters like updates_received)
  // are not to be cleared here.
  SyncStatus status = status_;
  status.server_conflicts = 0;
  status.committed_count = 0;
  return status;
}

SyncStatus SyncStatusTracker::CalcSyncing(const SyncCycleEvent& event) const {
  SyncStatus status = CreateBlankStatus();
  const SyncCycleSnapshot& snapshot = event.snapshot;
  status.server_conflicts = snapshot.num_server_conflicts();
  status.committed_count =
      snapshot.model_neutral_state().num_successful_commits;

  switch (event.what_happened) {
    case SyncCycleEvent::SYNC_CYCLE_BEGIN:
      status.syncing = true;
      break;
    case SyncCycleEvent::SYNC_CYCLE_ENDED:
      status.syncing = false;
      // Accumulate update count only once per cycle to avoid double-counting.
      status.updates_received +=
          snapshot.model_neutral_state().num_updates_downloaded_total;
      status.tombstone_updates_received +=
          snapshot.model_neutral_state().num_tombstone_updates_downloaded_total;
      status.num_commits_total +=
          snapshot.model_neutral_state().num_successful_commits;
      break;
    case SyncCycleEvent::STATUS_CHANGED:
      break;
  }
  return status;
}

void SyncStatusTracker::OnSyncCycleEvent(const SyncCycleEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_ = CalcSyncing(event);
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::OnActionableProtocolError(
    const SyncProtocolError& sync_protocol_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_ = CreateBlankStatus();
  status_.sync_protocol_error = sync_protocol_error;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::OnRetryTimeChanged(base::Time retry_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.retry_time = retry_time;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::OnThrottledTypesChanged(DataTypeSet throttled_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.throttled_types = throttled_types;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::OnBackedOffTypesChanged(DataTypeSet backed_off_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.backed_off_types = backed_off_types;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::OnMigrationRequested(DataTypeSet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SyncStatusTracker::OnProtocolEvent(const ProtocolEvent&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SyncStatusTracker::SetNotificationsEnabled(bool notifications_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.notifications_enabled = notifications_enabled;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::IncrementNotificationsReceived() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++status_.notifications_received;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetEncryptedTypes(DataTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.encrypted_types = types;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetCryptographerCanEncrypt(bool can_encrypt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.cryptographer_can_encrypt = can_encrypt;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetCryptoHasPendingKeys(bool has_pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.crypto_has_pending_keys = has_pending_keys;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetPassphraseType(PassphraseType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.passphrase_type = type;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetHasKeystoreKey(bool has_keystore_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.has_keystore_key = has_keystore_key;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetKeystoreMigrationTime(
    const base::Time& migration_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.keystore_migration_time = migration_time;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetTrustedVaultDebugInfo(
    const sync_pb::NigoriSpecifics::TrustedVaultDebugInfo&
        trusted_vault_debug_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.trusted_vault_debug_info = trusted_vault_debug_info;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetCacheGuid(const std::string& cache_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.cache_guid = cache_guid;
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetHasPendingInvalidations(
    DataType type,
    bool has_pending_invalidations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_pending_invalidations) {
    status_.invalidated_data_types.Put(type);
  } else {
    status_.invalidated_data_types.Remove(type);
  }
  status_changed_callback_.Run(status_);
}

void SyncStatusTracker::SetLocalBackendFolder(const std::string& folder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.local_sync_folder = folder;
  status_changed_callback_.Run(status_);
}

}  // namespace syncer
