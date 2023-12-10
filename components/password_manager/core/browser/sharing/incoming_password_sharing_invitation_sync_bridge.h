// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_INCOMING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_INCOMING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MetadataBatch;
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace password_manager {

class PasswordReceiverService;

// Sync bridge implementation for INCOMING_PASSWORD_SHARING_INVITATION model
// type.
class IncomingPasswordSharingInvitationSyncBridge
    : public syncer::ModelTypeSyncBridge {
 public:
  IncomingPasswordSharingInvitationSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_sync_metadata_store_callback);
  IncomingPasswordSharingInvitationSyncBridge(
      const IncomingPasswordSharingInvitationSyncBridge&) = delete;
  IncomingPasswordSharingInvitationSyncBridge& operator=(
      const IncomingPasswordSharingInvitationSyncBridge&) = delete;
  ~IncomingPasswordSharingInvitationSyncBridge() override;

  // |password_receiver_service| must outlive this object and must not be null.
  // This method must be called before any interaction with the server has
  // started.
  void SetPasswordReceiverService(
      PasswordReceiverService* password_receiver_service);

  // ModelTypeSyncBridge implementation.
  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override;
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 private:
  // Methods used as callbacks given to DataTypeStore.
  void OnModelTypeStoreCreated(const std::optional<syncer::ModelError>& error,
                               std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommitSyncMetadata(const std::optional<syncer::ModelError>& error);

  // Persists the changes to sync metadata store.
  void CommitSyncMetadata(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  // Stores only sync metadata.
  std::unique_ptr<syncer::ModelTypeStore> sync_metadata_store_;

  // Used to process incoming invitations.
  raw_ptr<PasswordReceiverService> password_receiver_service_ = nullptr;

  base::WeakPtrFactory<IncomingPasswordSharingInvitationSyncBridge>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_INCOMING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
