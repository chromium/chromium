// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/debug_info_event_listener.h"

#include <stddef.h>

#include "base/logging.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/protocol/client_debug_info.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"

namespace syncer {

DebugInfoEventListener::DebugInfoEventListener() = default;

DebugInfoEventListener::~DebugInfoEventListener() = default;

void DebugInfoEventListener::InitializationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
}

void DebugInfoEventListener::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  sync_pb::SyncCycleCompletedEventInfo* sync_completed_event_info =
      event_info.mutable_sync_cycle_completed_event_info();

  sync_completed_event_info->set_num_server_conflicts(
      snapshot.num_server_conflicts());

  sync_completed_event_info->set_num_updates_downloaded(
      snapshot.model_neutral_state().num_updates_downloaded_total);
  sync_completed_event_info->set_get_updates_origin(
      snapshot.get_updates_origin());
  sync_completed_event_info->mutable_caller_info()->set_notifications_enabled(
      snapshot.notifications_enabled());

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::CONNECTION_STATUS_CHANGE);
}

void DebugInfoEventListener::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::PASSPHRASE_REQUIRED);
}

void DebugInfoEventListener::OnPassphraseAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::PASSPHRASE_ACCEPTED);
}

void DebugInfoEventListener::OnTrustedVaultKeyRequired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::TRUSTED_VAULT_KEY_REQUIRED);
}

void DebugInfoEventListener::OnTrustedVaultKeyAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::TRUSTED_VAULT_KEY_ACCEPTED);
}

void DebugInfoEventListener::OnEncryptedTypesChanged(
    DataTypeSet encrypted_types,
    bool encrypt_everything) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ENCRYPTED_TYPES_CHANGED);
}

void DebugInfoEventListener::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cryptographer_has_pending_keys_ = has_pending_keys;
  cryptographer_can_encrypt_ = cryptographer->CanEncrypt();
}

void DebugInfoEventListener::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::PASSPHRASE_TYPE_CHANGED);
}

void DebugInfoEventListener::OnActionableProtocolError(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ACTIONABLE_ERROR);
}

void DebugInfoEventListener::OnMigrationRequested(DataTypeSet types) {}

void DebugInfoEventListener::OnProtocolEvent(const ProtocolEvent& event) {}

void DebugInfoEventListener::OnSyncStatusChanged(const SyncStatus& status) {}

void DebugInfoEventListener::OnNudgeFromDatatype(DataType datatype) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  event_info.set_nudging_datatype(
      GetSpecificsFieldNumberFromDataType(datatype));
  AddEventToQueue(event_info);
}

sync_pb::DebugInfo DebugInfoEventListener::GetDebugInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(events_.size(), kMaxEvents);

  sync_pb::DebugInfo debug_info;
  for (const sync_pb::DebugEventInfo& event : events_) {
    *debug_info.add_events() = event;
  }

  debug_info.set_events_dropped(events_dropped_);
  debug_info.set_cryptographer_ready(cryptographer_can_encrypt_);
  debug_info.set_cryptographer_has_pending_keys(
      cryptographer_has_pending_keys_);
  return debug_info;
}

void DebugInfoEventListener::ClearDebugInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(events_.size(), kMaxEvents);

  events_.clear();
  events_dropped_ = false;
}

void DebugInfoEventListener::CreateAndAddEvent(
    sync_pb::SyncEnums::SingletonDebugEventType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  event_info.set_singleton_event(type);
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::AddEventToQueue(
    const sync_pb::DebugEventInfo& event_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (events_.size() >= kMaxEvents) {
    DVLOG(1) << "DebugInfoEventListener::AddEventToQueue Dropping an old event "
             << "because of full queue";

    events_.pop_front();
    events_dropped_ = true;
  }
  events_.push_back(event_info);
}

}  // namespace syncer
