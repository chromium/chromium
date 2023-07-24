// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/sync/model/dummy_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"

namespace password_manager {

namespace {

syncer::ClientTagHash GetClientTagHashFromStorageKey(
    const std::string& storage_key) {
  return syncer::ClientTagHash::FromUnhashed(
      syncer::OUTGOING_PASSWORD_SHARING_INVITATION, storage_key);
}

std::string GetStorageKeyFromSpecifics(
    const sync_pb::OutgoingPasswordSharingInvitationSpecifics& specifics) {
  return specifics.guid();
}

sync_pb::OutgoingPasswordSharingInvitationSpecifics
CreateOutgoingPasswordSharingInvitationSpecifics(
    const PasswordForm& password_form,
    const PasswordRecipient& recipient) {
  sync_pb::OutgoingPasswordSharingInvitationSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_recipient_user_id(recipient.user_id);

  sync_pb::PasswordSharingInvitationData::PasswordData* password_data =
      specifics.mutable_client_only_unencrypted_data()->mutable_password_data();
  password_data->set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data->set_scheme(static_cast<int>(password_form.scheme));
  password_data->set_signon_realm(password_form.signon_realm);
  password_data->set_origin(
      password_form.url.is_valid() ? password_form.url.spec() : "");
  password_data->set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data->set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data->set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data->set_display_name(
      base::UTF16ToUTF8(password_form.display_name));
  password_data->set_avatar_url(
      password_form.icon_url.is_valid() ? password_form.icon_url.spec() : "");

  return specifics;
}

std::unique_ptr<syncer::EntityData> ConvertToEntityData(
    const sync_pb::OutgoingPasswordSharingInvitationSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = specifics.guid();
  entity_data->client_tag_hash =
      GetClientTagHashFromStorageKey(GetStorageKeyFromSpecifics(specifics));
  entity_data->specifics.mutable_outgoing_password_sharing_invitation()
      ->CopyFrom(specifics);
  return entity_data;
}

}  // namespace

OutgoingPasswordSharingInvitationSyncBridge::
    OutgoingPasswordSharingInvitationSyncBridge(
        std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  // Current data type doesn't have persistent storage so it's ready to sync
  // immediately.
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

OutgoingPasswordSharingInvitationSyncBridge::
    ~OutgoingPasswordSharingInvitationSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
OutgoingPasswordSharingInvitationSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The data type intentionally doesn't persist the data on disk, so metadata
  // is just ignored.
  return std::make_unique<syncer::DummyMetadataChangeList>();
}

void OutgoingPasswordSharingInvitationSyncBridge::SendPassword(
    const PasswordForm& password,
    const PasswordRecipient& recipient) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  sync_pb::OutgoingPasswordSharingInvitationSpecifics specifics =
      CreateOutgoingPasswordSharingInvitationSpecifics(password, recipient);
  std::string storage_key = GetStorageKeyFromSpecifics(specifics);

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(storage_key, ConvertToEntityData(specifics),
                          metadata_change_list.get());

  storage_key_to_outgoing_invitations_in_flight_.emplace(std::move(storage_key),
                                                         std::move(specifics));
}

absl::optional<syncer::ModelError>
OutgoingPasswordSharingInvitationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_data.empty());
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
OutgoingPasswordSharingInvitationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    // For commit-only data type only |ACTION_DELETE| is expected.
    CHECK_EQ(syncer::EntityChange::ACTION_DELETE, change->type());

    storage_key_to_outgoing_invitations_in_flight_.erase(change->storage_key());
  }
  return absl::nullopt;
}

void OutgoingPasswordSharingInvitationSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (auto iter =
            storage_key_to_outgoing_invitations_in_flight_.find(storage_key);
        iter != storage_key_to_outgoing_invitations_in_flight_.end()) {
      batch->Put(storage_key, ConvertToEntityData(iter->second));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void OutgoingPasswordSharingInvitationSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& storage_key_and_outgoing_invitation :
       storage_key_to_outgoing_invitations_in_flight_) {
    batch->Put(storage_key_and_outgoing_invitation.first,
               ConvertToEntityData(storage_key_and_outgoing_invitation.second));
  }
  std::move(callback).Run(std::move(batch));
}

std::string OutgoingPasswordSharingInvitationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string OutgoingPasswordSharingInvitationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetStorageKeyFromSpecifics(
      entity_data.specifics.outgoing_password_sharing_invitation());
}

bool OutgoingPasswordSharingInvitationSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool OutgoingPasswordSharingInvitationSyncBridge::SupportsGetStorageKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void OutgoingPasswordSharingInvitationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_key_to_outgoing_invitations_in_flight_.clear();
}

}  // namespace password_manager
