// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_sync_bridge_impl.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"

namespace consent_auditor {

using sync_pb::UserConsentSpecifics;
using syncer::DataTypeLocalChangeProcessor;
using syncer::DataTypeStore;
using syncer::DataTypeSyncBridge;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataBatch;
using syncer::MetadataChangeList;
using syncer::ModelError;
using syncer::MutableDataBatch;
using syncer::OnceDataTypeStoreFactory;

namespace {

std::string GetStorageKeyFromSpecifics(const UserConsentSpecifics& specifics) {
  // Force Big Endian, this means newly created keys are last in sort order,
  // which allows leveldb to append new writes, which it is best at.
  // TODO(skym): Until we force |event_time_usec| to never conflict, this has
  // the potential for errors.
  std::string key(8u, char{0});
  base::as_writable_byte_span(key).copy_from(base::U64ToBigEndian(
      base::checked_cast<uint64_t>(specifics.client_consent_time_usec())));
  return key;
}

std::unique_ptr<EntityData> MoveToEntityData(
    std::unique_ptr<UserConsentSpecifics> specifics) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name =
      base::NumberToString(specifics->client_consent_time_usec());
  entity_data->specifics.set_allocated_user_consent(specifics.release());
  return entity_data;
}

}  // namespace

ConsentSyncBridgeImpl::ConsentSyncBridgeImpl(
    OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)) {
  StoreWithCache::CreateAndLoad(
      std::move(store_factory), syncer::USER_CONSENTS,
      base::BindOnce(&ConsentSyncBridgeImpl::OnStoreLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

ConsentSyncBridgeImpl::~ConsentSyncBridgeImpl() {
  if (!deferred_consents_while_initializing_.empty()) {
    LOG(ERROR) << "Non-empty event queue at shutdown!";
  }
  // TODO(crbug.com/362428820): Remove logging once investigation is complete.
  if (store_) {
    VLOG(1) << "UserConsents during destruction: "
            << store_->in_memory_data().size();
  }
}

std::unique_ptr<MetadataChangeList>
ConsentSyncBridgeImpl::CreateMetadataChangeList() {
  return DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<ModelError> ConsentSyncBridgeImpl::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(entity_data.empty());
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(!change_processor()->TrackedAccountId().empty());
  ResubmitAllData();
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<ModelError> ConsentSyncBridgeImpl::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  CHECK(store_);

  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    DCHECK_EQ(EntityChange::ACTION_DELETE, change->type());
    batch->DeleteData(change->storage_key());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&ConsentSyncBridgeImpl::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  return {};
}

std::unique_ptr<syncer::DataBatch> ConsentSyncBridgeImpl::GetDataForCommit(
    StorageKeyList storage_keys) {
  CHECK(store_);

  auto batch = std::make_unique<MutableDataBatch>();
  const std::map<std::string, UserConsentSpecifics>& in_memory_data =
      store_->in_memory_data();
  for (const std::string& storage_key : storage_keys) {
    auto it = in_memory_data.find(storage_key);
    if (it != in_memory_data.end()) {
      auto specifics = std::make_unique<UserConsentSpecifics>(it->second);
      batch->Put(it->first, MoveToEntityData(std::move(specifics)));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
ConsentSyncBridgeImpl::GetAllDataForDebugging() {
  CHECK(store_);

  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& [storage_key, specifics] : store_->in_memory_data()) {
    auto specifics_copy = std::make_unique<UserConsentSpecifics>(specifics);
    batch->Put(storage_key, MoveToEntityData(std::move(specifics_copy)));
  }
  return batch;
}

std::string ConsentSyncBridgeImpl::GetClientTag(const EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string ConsentSyncBridgeImpl::GetStorageKey(
    const EntityData& entity_data) {
  return GetStorageKeyFromSpecifics(entity_data.specifics.user_consent());
}

void ConsentSyncBridgeImpl::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Sync can only be stopped after initialization.
  DCHECK(deferred_consents_while_initializing_.empty());
  CHECK(store_);

  // Preserve all consents in the store, but delete their metadata, because it
  // may become invalid when sync is reenabled. It is important to report all
  // user consents, thus, they are persisted for some time even after signout.
  // The bridge will try to resubmit these consents once sync is enabled again.
  // This may lead to same consent being submitted multiple times, but this is
  // allowed.
  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->TakeMetadataChangesFrom(std::move(delete_metadata_change_list));

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&ConsentSyncBridgeImpl::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ConsentSyncBridgeImpl::ResubmitAllData() {
  DCHECK(!change_processor()->TrackedAccountId().empty());
  DCHECK(change_processor()->IsTrackingMetadata());
  CHECK(store_);

  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const auto& [storage_key, specifics] : store_->in_memory_data()) {
    if (specifics.account_id() == change_processor()->TrackedAccountId()) {
      auto specifics_copy = std::make_unique<UserConsentSpecifics>(specifics);
      change_processor()->Put(storage_key,
                              MoveToEntityData(std::move(specifics_copy)),
                              batch->GetMetadataChangeList());
    }
  }

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&ConsentSyncBridgeImpl::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ConsentSyncBridgeImpl::RecordConsent(
    std::unique_ptr<UserConsentSpecifics> specifics) {
  // TODO(vitaliii): Sanity-check specifics->account_id() against
  // change_processor()->TrackedAccountId(), maybe DCHECK.
  DCHECK(!specifics->account_id().empty());
  if (store_) {
    RecordConsentImpl(std::move(specifics));
    return;
  }
  deferred_consents_while_initializing_.push_back(std::move(specifics));
}

// static
std::string ConsentSyncBridgeImpl::GetStorageKeyFromSpecificsForTest(
    const UserConsentSpecifics& specifics) {
  return GetStorageKeyFromSpecifics(specifics);
}

std::unique_ptr<DataTypeStore> ConsentSyncBridgeImpl::StealStoreForTest() {
  return StoreWithCache::ExtractUnderlyingStoreForTest(std::move(store_));
}

void ConsentSyncBridgeImpl::RecordConsentImpl(
    std::unique_ptr<UserConsentSpecifics> specifics) {
  CHECK(store_);

  std::string storage_key = GetStorageKeyFromSpecifics(*specifics);
  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(storage_key, *specifics);

  if (specifics->account_id() == change_processor()->TrackedAccountId()) {
    change_processor()->Put(storage_key, MoveToEntityData(std::move(specifics)),
                            batch->GetMetadataChangeList());
  }

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&ConsentSyncBridgeImpl::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ConsentSyncBridgeImpl::GetControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

void ConsentSyncBridgeImpl::ProcessQueuedEvents() {
  for (std::unique_ptr<UserConsentSpecifics>& event :
       deferred_consents_while_initializing_) {
    RecordConsentImpl(std::move(event));
  }
  deferred_consents_while_initializing_.clear();
}

void ConsentSyncBridgeImpl::OnStoreLoaded(
    const std::optional<ModelError>& error,
    std::unique_ptr<StoreWithCache> store,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "ConsentSyncBridgeImpl::OnStoreLoaded");
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  if (!change_processor()->TrackedAccountId().empty()) {
    // Resubmit all data in case the client crashed immediately after
    // MergeFullSyncData(), where submissions are supposed to happen and
    // metadata populated.
    ResubmitAllData();
  }
  ProcessQueuedEvents();
}

void ConsentSyncBridgeImpl::OnStoreCommit(
    const std::optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace consent_auditor
