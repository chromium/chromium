// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_item_sync_bridge.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/empty_metadata_change_list.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/upload_outcome.h"

namespace sync_tab_context {
namespace {

std::string GetClientTag(const ContainerId& container_id,
                         const std::string& item_id) {
  return base::StrCat({container_id.value().AsLowercaseString(), ":", item_id});
}

std::string GetStorageKeyFromSpecifics(
    const sync_pb::EncryptedTabContextItemSpecifics& specifics) {
  return GetClientTag(
      ContainerId(base::Uuid::ParseCaseInsensitive(specifics.container_id())),
      specifics.item_id());
}

syncer::ClientTagHash GetClientTagHashFromStorageKey(
    const std::string& storage_key) {
  return syncer::ClientTagHash::FromUnhashed(syncer::ENCRYPTED_TAB_CONTEXT_ITEM,
                                             storage_key);
}

std::unique_ptr<syncer::EntityData> ConvertToEntityData(
    const sync_pb::EncryptedTabContextItemSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = GetStorageKeyFromSpecifics(specifics);
  *entity_data->specifics.mutable_encrypted_tab_context_item() = specifics;
  return entity_data;
}

}  // namespace

TabContextItemSyncBridge::PendingCommit::PendingCommit() = default;
TabContextItemSyncBridge::PendingCommit::PendingCommit(PendingCommit&&) =
    default;
TabContextItemSyncBridge::PendingCommit&
TabContextItemSyncBridge::PendingCommit::operator=(PendingCommit&&) = default;
TabContextItemSyncBridge::PendingCommit::~PendingCommit() = default;

TabContextItemSyncBridge::TabContextItemSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

TabContextItemSyncBridge::~TabContextItemSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FailAllPendingCommits();
}

void TabContextItemSyncBridge::UploadItem(
    const ContainerId& container_id,
    const std::string& item_id,
    sync_pb::EncryptedData encrypted_content,
    UploadCompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(container_id.value().is_valid());
  CHECK(!item_id.empty());
  CHECK(callback);
  if (!change_processor()->IsTrackingMetadata()) {
    std::move(callback).Run(UploadOutcome::kFailed);
    return;
  }

  sync_pb::EncryptedTabContextItemSpecifics specifics;
  specifics.set_container_id(container_id.value().AsLowercaseString());
  specifics.set_item_id(item_id);
  *specifics.mutable_encrypted_content() = std::move(encrypted_content);

  const std::string storage_key = GetStorageKeyFromSpecifics(specifics);
  const syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromStorageKey(storage_key);

  PendingCommit& pending_commit = pending_commits_[client_tag_hash];
  pending_commit.specifics = std::move(specifics);
  pending_commit.callbacks.push_back(std::move(callback));

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(storage_key,
                          ConvertToEntityData(pending_commit.specifics),
                          metadata_change_list.get());
}

std::unique_ptr<syncer::MetadataChangeList>
TabContextItemSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The data type intentionally doesn't persist data or metadata on disk, so
  // metadata changes are ignored.
  return std::make_unique<syncer::EmptyMetadataChangeList>();
}

std::optional<syncer::ModelError> TabContextItemSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::nullopt;
}

std::optional<syncer::ModelError>
TabContextItemSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    // For commit-only data types, the processor emits ACTION_DELETE once an
    // entity has been committed and has no remaining unsynced changes.
    CHECK_EQ(syncer::EntityChange::ACTION_DELETE, change->type());
    NotifyCallbacksForClientTagHash(
        GetClientTagHashFromStorageKey(change->storage_key()),
        UploadOutcome::kSucceeded);
  }
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> TabContextItemSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    const PendingCommit* pending_commit = base::FindOrNull(
        pending_commits_, GetClientTagHashFromStorageKey(storage_key));
    if (pending_commit) {
      batch->Put(storage_key, ConvertToEntityData(pending_commit->specifics));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
TabContextItemSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::pair<syncer::ClientTagHash, PendingCommit>& entry :
       pending_commits_) {
    batch->Put(GetStorageKeyFromSpecifics(entry.second.specifics),
               ConvertToEntityData(entry.second.specifics));
  }
  return batch;
}

std::string TabContextItemSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetStorageKeyFromSpecifics(
      entity_data.specifics.encrypted_tab_context_item());
}

std::string TabContextItemSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

sync_pb::EntitySpecifics
TabContextItemSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_pb::EntitySpecifics();
}

bool TabContextItemSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const sync_pb::EncryptedTabContextItemSpecifics& specifics =
      entity_data.specifics.encrypted_tab_context_item();
  return base::Uuid::ParseCaseInsensitive(specifics.container_id())
             .is_valid() &&
         !specifics.item_id().empty() && specifics.has_encrypted_content();
}

void TabContextItemSyncBridge::OnCommitAttemptErrors(
    const syncer::FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const syncer::FailedCommitResponseData& response : error_response_list) {
    // Only allow the processor to retry `TRANSIENT_ERROR` on the next sync
    // cycle; treat all other per-item error responses as permanent and
    // untrack/fail them immediately.
    if (response.response_type != sync_pb::CommitResponse::TRANSIENT_ERROR) {
      change_processor()->UntrackEntityForClientTagHash(
          response.client_tag_hash);
      NotifyCallbacksForClientTagHash(response.client_tag_hash,
                                      UploadOutcome::kFailed);
    }
  }
}

syncer::DataTypeSyncBridge::CommitAttemptFailedBehavior
TabContextItemSyncBridge::OnCommitAttemptFailed(
    syncer::SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (commit_error) {
    case syncer::SyncCommitError::kNetworkError:
    case syncer::SyncCommitError::kAuthError:
      return CommitAttemptFailedBehavior::kShouldRetryOnNextCycle;
    case syncer::SyncCommitError::kServerError:
    case syncer::SyncCommitError::kBadServerResponse:
      for (const std::pair<syncer::ClientTagHash, PendingCommit>& entry :
           pending_commits_) {
        change_processor()->UntrackEntityForClientTagHash(entry.first);
      }
      FailAllPendingCommits();
      return CommitAttemptFailedBehavior::kDontRetryOnNextCycle;
  }
  NOTREACHED();
}

void TabContextItemSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FailAllPendingCommits();
}

void TabContextItemSyncBridge::NotifyCallbacksForClientTagHash(
    const syncer::ClientTagHash& client_tag_hash,
    UploadOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PendingCommit* pending_commit =
      base::FindOrNull(pending_commits_, client_tag_hash);
  if (!pending_commit) {
    return;
  }
  std::vector<UploadCompletionCallback> callbacks =
      std::move(pending_commit->callbacks);
  pending_commits_.erase(client_tag_hash);
  for (UploadCompletionCallback& callback : callbacks) {
    std::move(callback).Run(outcome);
  }
}

void TabContextItemSyncBridge::FailAllPendingCommits() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_map<syncer::ClientTagHash, PendingCommit> pending_commits =
      std::move(pending_commits_);
  pending_commits_.clear();
  for (std::pair<syncer::ClientTagHash, PendingCommit>& entry :
       pending_commits) {
    for (UploadCompletionCallback& callback : entry.second.callbacks) {
      std::move(callback).Run(UploadOutcome::kFailed);
    }
  }
}

}  // namespace sync_tab_context
