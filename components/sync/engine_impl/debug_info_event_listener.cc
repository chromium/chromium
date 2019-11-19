// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/debug_info_event_listener.h"

#include <stddef.h>

#include "components/sync/nigori/cryptographer.h"

namespace syncer {

DebugInfoEventListener::DebugInfoEventListener()
    : events_dropped_(false),
      cryptographer_has_pending_keys_(false),
      cryptographer_can_encrypt_(false) {}

DebugInfoEventListener::~DebugInfoEventListener() {}

void DebugInfoEventListener::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  sync_pb::SyncCycleCompletedEventInfo* sync_completed_event_info =
      event_info.mutable_sync_cycle_completed_event_info();

  sync_completed_event_info->set_num_encryption_conflicts(
      snapshot.num_encryption_conflicts());
  sync_completed_event_info->set_num_hierarchy_conflicts(
      snapshot.num_hierarchy_conflicts());
  sync_completed_event_info->set_num_server_conflicts(
      snapshot.num_server_conflicts());

  sync_completed_event_info->set_num_updates_downloaded(
      snapshot.model_neutral_state().num_updates_downloaded_total);
  sync_completed_event_info->set_num_reflected_updates_downloaded(
      snapshot.model_neutral_state().num_reflected_updates_downloaded_total);
  sync_completed_event_info->set_get_updates_origin(
      snapshot.get_updates_origin());
  sync_completed_event_info->mutable_caller_info()->set_notifications_enabled(
      snapshot.notifications_enabled());

  // Fill the legacy GetUpdatesSource field. This is not used anymore, but it's
  // a required field so we still have to fill it with something.
  sync_completed_event_info->mutable_caller_info()->set_source(
      sync_pb::GetUpdatesCallerInfo::UNKNOWN);

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    const WeakHandle<DataTypeDebugInfoListener>& debug_listener,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
}

void DebugInfoEventListener::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::CONNECTION_STATUS_CHANGE);
}

void DebugInfoEventListener::OnPassphraseRequired(
    PassphraseRequiredReason reason,
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

void DebugInfoEventListener::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type == PASSPHRASE_BOOTSTRAP_TOKEN) {
    CreateAndAddEvent(sync_pb::SyncEnums::BOOTSTRAP_TOKEN_UPDATED);
    return;
  }
  DCHECK_EQ(type, KEYSTORE_BOOTSTRAP_TOKEN);
  CreateAndAddEvent(sync_pb::SyncEnums::KEYSTORE_TOKEN_UPDATED);
}

void DebugInfoEventListener::OnEncryptedTypesChanged(
    ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ENCRYPTED_TYPES_CHANGED);
}

void DebugInfoEventListener::OnEncryptionComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ENCRYPTION_COMPLETE);
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

void DebugInfoEventListener::OnActionableError(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ACTIONABLE_ERROR);
}

void DebugInfoEventListener::OnMigrationRequested(ModelTypeSet types) {}

void DebugInfoEventListener::OnProtocolEvent(const ProtocolEvent& event) {}

void DebugInfoEventListener::OnNudgeFromDatatype(ModelType datatype) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  event_info.set_nudging_datatype(
      GetSpecificsFieldNumberFromModelType(datatype));
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::GetDebugInfo(sync_pb::DebugInfo* debug_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(events_.size(), kMaxEntries);

  for (DebugEventInfoQueue::const_iterator iter = events_.begin();
       iter != events_.end(); ++iter) {
    sync_pb::DebugEventInfo* event_info = debug_info->add_events();
    event_info->CopyFrom(*iter);
  }

  debug_info->set_events_dropped(events_dropped_);
  debug_info->set_cryptographer_ready(cryptographer_can_encrypt_);
  debug_info->set_cryptographer_has_pending_keys(
      cryptographer_has_pending_keys_);
}

void DebugInfoEventListener::ClearDebugInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(events_.size(), kMaxEntries);

  events_.clear();
  events_dropped_ = false;
}

base::WeakPtr<DataTypeDebugInfoListener> DebugInfoEventListener::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void DebugInfoEventListener::OnDataTypeConfigureComplete(
    const std::vector<DataTypeConfigurationStats>& configuration_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < configuration_stats.size(); ++i) {
    DCHECK(ProtocolTypes().Has(configuration_stats[i].model_type));
    const DataTypeAssociationStats& association_stats =
        configuration_stats[i].association_stats;

    sync_pb::DebugEventInfo association_event;
    sync_pb::DatatypeAssociationStats* datatype_stats =
        association_event.mutable_datatype_association_stats();
    datatype_stats->set_data_type_id(GetSpecificsFieldNumberFromModelType(
        configuration_stats[i].model_type));
    datatype_stats->set_num_local_items_before_association(
        association_stats.num_local_items_before_association);
    datatype_stats->set_num_sync_items_before_association(
        association_stats.num_sync_items_before_association);
    datatype_stats->set_num_local_items_after_association(
        association_stats.num_local_items_after_association);
    datatype_stats->set_num_sync_items_after_association(
        association_stats.num_sync_items_after_association);
    datatype_stats->set_num_local_items_added(
        association_stats.num_local_items_added);
    datatype_stats->set_num_local_items_deleted(
        association_stats.num_local_items_deleted);
    datatype_stats->set_num_local_items_modified(
        association_stats.num_local_items_modified);
    datatype_stats->set_num_sync_items_added(
        association_stats.num_sync_items_added);
    datatype_stats->set_num_sync_items_deleted(
        association_stats.num_sync_items_deleted);
    datatype_stats->set_num_sync_items_modified(
        association_stats.num_sync_items_modified);
    datatype_stats->set_local_version_pre_association(
        association_stats.local_version_pre_association);
    datatype_stats->set_sync_version_pre_association(
        association_stats.sync_version_pre_association);
    datatype_stats->set_had_error(association_stats.had_error);
    datatype_stats->set_association_wait_time_for_same_priority_us(
        association_stats.association_wait_time.InMicroseconds());
    datatype_stats->set_association_time_us(
        association_stats.association_time.InMicroseconds());
    datatype_stats->set_download_wait_time_us(
        configuration_stats[i].download_wait_time.InMicroseconds());
    datatype_stats->set_download_time_us(
        configuration_stats[i].download_time.InMicroseconds());
    datatype_stats->set_association_wait_time_for_high_priority_us(
        configuration_stats[i]
            .association_wait_time_for_high_priority.InMicroseconds());

    for (ModelType type :
         configuration_stats[i].high_priority_types_configured_before) {
      datatype_stats->add_high_priority_type_configured_before(
          GetSpecificsFieldNumberFromModelType(type));
    }

    for (ModelType type :
         configuration_stats[i].same_priority_types_configured_before) {
      datatype_stats->add_same_priority_type_configured_before(
          GetSpecificsFieldNumberFromModelType(type));
    }

    AddEventToQueue(association_event);
  }
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
  if (events_.size() >= kMaxEntries) {
    DVLOG(1) << "DebugInfoEventListener::AddEventToQueue Dropping an old event "
             << "because of full queue";

    events_.pop_front();
    events_dropped_ = true;
  }
  events_.push_back(event_info);
}

}  // namespace syncer
