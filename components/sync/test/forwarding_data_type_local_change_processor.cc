// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/forwarding_data_type_local_change_processor.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace syncer {

ForwardingDataTypeLocalChangeProcessor::ForwardingDataTypeLocalChangeProcessor(
    DataTypeLocalChangeProcessor* other)
    : other_(other) {}
ForwardingDataTypeLocalChangeProcessor::
    ~ForwardingDataTypeLocalChangeProcessor() = default;

void ForwardingDataTypeLocalChangeProcessor::Put(
    const std::string& client_tag,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_change_list) {
  other_->Put(client_tag, std::move(entity_data), metadata_change_list);
}

void ForwardingDataTypeLocalChangeProcessor::Delete(
    const std::string& client_tag,
    const DeletionOrigin& origin,
    MetadataChangeList* metadata_change_list) {
  other_->Delete(client_tag, origin, metadata_change_list);
}

void ForwardingDataTypeLocalChangeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  other_->UpdateStorageKey(entity_data, storage_key, metadata_change_list);
}

void ForwardingDataTypeLocalChangeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  other_->UntrackEntityForStorageKey(storage_key);
}

void ForwardingDataTypeLocalChangeProcessor::UntrackEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {
  other_->UntrackEntityForClientTagHash(client_tag_hash);
}

std::vector<std::string>
ForwardingDataTypeLocalChangeProcessor::GetAllTrackedStorageKeys() const {
  return other_->GetAllTrackedStorageKeys();
}

bool ForwardingDataTypeLocalChangeProcessor::IsEntityUnsynced(
    const std::string& storage_key) const {
  return other_->IsEntityUnsynced(storage_key);
}

base::Time ForwardingDataTypeLocalChangeProcessor::GetEntityCreationTime(
    const std::string& storage_key) const {
  return other_->GetEntityCreationTime(storage_key);
}

base::Time ForwardingDataTypeLocalChangeProcessor::GetEntityModificationTime(
    const std::string& storage_key) const {
  return other_->GetEntityModificationTime(storage_key);
}

void ForwardingDataTypeLocalChangeProcessor::OnModelStarting(
    DataTypeSyncBridge* bridge) {
  other_->OnModelStarting(bridge);
}

void ForwardingDataTypeLocalChangeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {
  other_->ModelReadyToSync(std::move(batch));
}

bool ForwardingDataTypeLocalChangeProcessor::IsTrackingMetadata() const {
  return other_->IsTrackingMetadata();
}

std::string ForwardingDataTypeLocalChangeProcessor::TrackedAccountId() const {
  return other_->TrackedAccountId();
}

std::string ForwardingDataTypeLocalChangeProcessor::TrackedCacheGuid() const {
  return other_->TrackedCacheGuid();
}

void ForwardingDataTypeLocalChangeProcessor::ReportError(
    const ModelError& error) {
  other_->ReportError(error);
}

std::optional<ModelError> ForwardingDataTypeLocalChangeProcessor::GetError()
    const {
  return other_->GetError();
}

base::WeakPtr<DataTypeControllerDelegate>
ForwardingDataTypeLocalChangeProcessor::GetControllerDelegate() {
  return other_->GetControllerDelegate();
}

const sync_pb::EntitySpecifics&
ForwardingDataTypeLocalChangeProcessor::GetPossiblyTrimmedRemoteSpecifics(
    const std::string& storage_key) const {
  return other_->GetPossiblyTrimmedRemoteSpecifics(storage_key);
}

sync_pb::UniquePosition
ForwardingDataTypeLocalChangeProcessor::UniquePositionAfter(
    const std::string& storage_key_before,
    const ClientTagHash& target_client_tag_hash) const {
  return other_->UniquePositionAfter(storage_key_before,
                                     target_client_tag_hash);
}

sync_pb::UniquePosition
ForwardingDataTypeLocalChangeProcessor::UniquePositionBefore(
    const std::string& storage_key_after,
    const ClientTagHash& target_client_tag_hash) const {
  return other_->UniquePositionBefore(storage_key_after,
                                      target_client_tag_hash);
}

sync_pb::UniquePosition
ForwardingDataTypeLocalChangeProcessor::UniquePositionBetween(
    const std::string& storage_key_before,
    const std::string& storage_key_after,
    const ClientTagHash& target_client_tag_hash) const {
  return other_->UniquePositionBetween(storage_key_before, storage_key_after,
                                       target_client_tag_hash);
}

sync_pb::UniquePosition
ForwardingDataTypeLocalChangeProcessor::UniquePositionForInitialEntity(
    const ClientTagHash& target_client_tag_hash) const {
  return other_->UniquePositionForInitialEntity(target_client_tag_hash);
}

sync_pb::UniquePosition
ForwardingDataTypeLocalChangeProcessor::GetUniquePositionForStorageKey(
    const std::string& storage_key) const {
  return other_->GetUniquePositionForStorageKey(storage_key);
}

base::WeakPtr<DataTypeLocalChangeProcessor>
ForwardingDataTypeLocalChangeProcessor::GetWeakPtr() {
  // Note: Don't bother with a separate WeakPtrFactory for the forwarding
  // processor; just hand out a WeakPtr directly to the real processor.
  return other_->GetWeakPtr();
}

}  // namespace syncer
