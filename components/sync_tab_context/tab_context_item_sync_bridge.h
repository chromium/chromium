// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_ITEM_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_ITEM_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/upload_outcome.h"

namespace syncer {
class DataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace sync_tab_context {

// Sync bridge that implements the commit-only datatype
// ENCRYPTED_TAB_CONTEXT_ITEM responsible for uploading individual encrypted
// blobs. Note that this data type uses custom encryption (via container
// encryption keys) rather than sync's built-in encryption infrastructure
// (Nigori).
class TabContextItemSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  using UploadCompletionCallback = base::OnceCallback<void(UploadOutcome)>;

  explicit TabContextItemSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  TabContextItemSyncBridge(const TabContextItemSyncBridge&) = delete;
  TabContextItemSyncBridge& operator=(const TabContextItemSyncBridge&) = delete;
  ~TabContextItemSyncBridge() override;

  // Uploads an item into the sync processor for commit and runs `callback` with
  // `UploadOutcome::kSucceeded` once the item is committed to the server, or
  // with `UploadOutcome::kFailed` if the commit fails or sync stops before
  // completion.
  void UploadItem(const ContainerId& container_id,
                  const std::string& item_id,
                  sync_pb::EncryptedData encrypted_content,
                  UploadCompletionCallback callback);

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  void OnCommitAttemptErrors(
      const syncer::FailedCommitResponseDataList& error_response_list) override;
  CommitAttemptFailedBehavior OnCommitAttemptFailed(
      syncer::SyncCommitError commit_error) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 private:
  struct PendingCommit {
    PendingCommit();
    PendingCommit(PendingCommit&&);
    PendingCommit& operator=(PendingCommit&&);
    ~PendingCommit();

    sync_pb::EncryptedTabContextItemSpecifics specifics;
    std::vector<UploadCompletionCallback> callbacks;
  };

  void NotifyCallbacksForClientTagHash(
      const syncer::ClientTagHash& client_tag_hash,
      UploadOutcome outcome);
  void FailAllPendingCommits();

  SEQUENCE_CHECKER(sequence_checker_);

  // In-flight commits indexed by entity `ClientTagHash`. Retaining `specifics`
  // until the commit finishes allows `GetDataForCommit()` to re-supply the
  // payload if the processor retries after a transient error. Multiple
  // callbacks may be queued for the same `ClientTagHash` if the same item is
  // updated again while a previous commit is still in flight.
  base::flat_map<syncer::ClientTagHash, PendingCommit> pending_commits_;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_ITEM_SYNC_BRIDGE_H_
