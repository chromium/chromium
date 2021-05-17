// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/all_status.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "components/sync/engine/sync_cycle_event.h"
#include "components/sync/engine/sync_status_observer.h"

namespace syncer {

AllStatus::AllStatus() {
  status_.notifications_enabled = false;
  status_.cryptographer_can_encrypt = false;
  status_.crypto_has_pending_keys = false;
}

AllStatus::~AllStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SyncStatus AllStatus::CreateBlankStatus() const {
  // Status is initialized with the previous status value.  Variables
  // whose values accumulate (e.g. lifetime counters like updates_received)
  // are not to be cleared here.
  SyncStatus status = status_;
  status.encryption_conflicts = 0;
  status.hierarchy_conflicts = 0;
  status.server_conflicts = 0;
  status.committed_count = 0;
  return status;
}

SyncStatus AllStatus::CalcSyncing(const SyncCycleEvent& event) const {
  SyncStatus status = CreateBlankStatus();
  const SyncCycleSnapshot& snapshot = event.snapshot;
  status.encryption_conflicts = snapshot.num_encryption_conflicts();
  status.hierarchy_conflicts = snapshot.num_hierarchy_conflicts();
  status.server_conflicts = snapshot.num_server_conflicts();
  status.committed_count =
      snapshot.model_neutral_state().num_successful_commits;
  status.num_entries_by_type = snapshot.num_entries_by_type();
  status.num_to_delete_entries_by_type =
      snapshot.num_to_delete_entries_by_type();

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
      status.reflected_updates_received +=
          snapshot.model_neutral_state().num_reflected_updates_downloaded_total;
      status.num_commits_total +=
          snapshot.model_neutral_state().num_successful_commits;
      status.num_local_overwrites_total +=
          snapshot.model_neutral_state().num_local_overwrites;
      status.num_server_overwrites_total +=
          snapshot.model_neutral_state().num_server_overwrites;
      break;
    case SyncCycleEvent::STATUS_CHANGED:
      break;
  }
  return status;
}

void AllStatus::AddObserver(SyncStatusObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  sync_status_observers_.push_back(observer);
  observer->OnSyncStatusChanged(status_);
}

void AllStatus::OnSyncCycleEvent(const SyncCycleEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_ = CalcSyncing(event);
  NotifyStatusChanged();
}

void AllStatus::OnActionableError(
    const SyncProtocolError& sync_protocol_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_ = CreateBlankStatus();
  status_.sync_protocol_error = sync_protocol_error;
  NotifyStatusChanged();
}

void AllStatus::OnRetryTimeChanged(base::Time retry_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.retry_time = retry_time;
  NotifyStatusChanged();
}

void AllStatus::OnThrottledTypesChanged(ModelTypeSet throttled_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.throttled_types = throttled_types;
  NotifyStatusChanged();
}

void AllStatus::OnBackedOffTypesChanged(ModelTypeSet backed_off_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.backed_off_types = backed_off_types;
  NotifyStatusChanged();
}

void AllStatus::OnMigrationRequested(ModelTypeSet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AllStatus::OnProtocolEvent(const ProtocolEvent&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AllStatus::SetNotificationsEnabled(bool notifications_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.notifications_enabled = notifications_enabled;
  NotifyStatusChanged();
}

void AllStatus::IncrementNotificationsReceived() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++status_.notifications_received;
  NotifyStatusChanged();
}

void AllStatus::SetEncryptedTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.encrypted_types = types;
  NotifyStatusChanged();
}

void AllStatus::SetCryptographerCanEncrypt(bool can_encrypt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.cryptographer_can_encrypt = can_encrypt;
  NotifyStatusChanged();
}

void AllStatus::SetCryptoHasPendingKeys(bool has_pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.crypto_has_pending_keys = has_pending_keys;
  NotifyStatusChanged();
}

void AllStatus::SetPassphraseType(PassphraseType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.passphrase_type = type;
  NotifyStatusChanged();
}

void AllStatus::SetHasKeystoreKey(bool has_keystore_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.has_keystore_key = has_keystore_key;
  NotifyStatusChanged();
}

void AllStatus::SetKeystoreMigrationTime(const base::Time& migration_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.keystore_migration_time = migration_time;
  NotifyStatusChanged();
}

void AllStatus::SetTrustedVaultDebugInfo(
    const sync_pb::NigoriSpecifics::TrustedVaultDebugInfo&
        trusted_vault_debug_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.trusted_vault_debug_info = trusted_vault_debug_info;
  NotifyStatusChanged();
}

void AllStatus::SetSyncId(const std::string& sync_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.sync_id = sync_id;
  NotifyStatusChanged();
}

void AllStatus::SetInvalidatorClientId(
    const std::string& invalidator_client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.invalidator_client_id = invalidator_client_id;
  NotifyStatusChanged();
}

void AllStatus::SetLocalBackendFolder(const std::string& folder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_.local_sync_folder = folder;
  NotifyStatusChanged();
}

void AllStatus::NotifyStatusChanged() {
  for (SyncStatusObserver* observer : sync_status_observers_) {
    observer->OnSyncStatusChanged(status_);
  }
}

}  // namespace syncer
