// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/forwarding_model_type_change_processor.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_data.h"

namespace syncer {

ForwardingModelTypeChangeProcessor::ForwardingModelTypeChangeProcessor(
    ModelTypeChangeProcessor* other)
    : other_(other) {}
ForwardingModelTypeChangeProcessor::~ForwardingModelTypeChangeProcessor() =
    default;

void ForwardingModelTypeChangeProcessor::Put(
    const std::string& client_tag,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_change_list) {
  other_->Put(client_tag, std::move(entity_data), metadata_change_list);
}

void ForwardingModelTypeChangeProcessor::Delete(
    const std::string& client_tag,
    MetadataChangeList* metadata_change_list) {
  other_->Delete(client_tag, metadata_change_list);
}

void ForwardingModelTypeChangeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  other_->UpdateStorageKey(entity_data, storage_key, metadata_change_list);
}

void ForwardingModelTypeChangeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  other_->UntrackEntityForStorageKey(storage_key);
}

void ForwardingModelTypeChangeProcessor::UntrackEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {
  other_->UntrackEntityForClientTagHash(client_tag_hash);
}

std::vector<std::string>
ForwardingModelTypeChangeProcessor::GetAllTrackedStorageKeys() const {
  return other_->GetAllTrackedStorageKeys();
}

bool ForwardingModelTypeChangeProcessor::IsEntityUnsynced(
    const std::string& storage_key) {
  return other_->IsEntityUnsynced(storage_key);
}

base::Time ForwardingModelTypeChangeProcessor::GetEntityCreationTime(
    const std::string& storage_key) const {
  return other_->GetEntityCreationTime(storage_key);
}

base::Time ForwardingModelTypeChangeProcessor::GetEntityModificationTime(
    const std::string& storage_key) const {
  return other_->GetEntityModificationTime(storage_key);
}

void ForwardingModelTypeChangeProcessor::OnModelStarting(
    ModelTypeSyncBridge* bridge) {
  other_->OnModelStarting(bridge);
}

void ForwardingModelTypeChangeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {
  other_->ModelReadyToSync(std::move(batch));
}

bool ForwardingModelTypeChangeProcessor::IsTrackingMetadata() const {
  return other_->IsTrackingMetadata();
}

std::string ForwardingModelTypeChangeProcessor::TrackedAccountId() const {
  return other_->TrackedAccountId();
}

std::string ForwardingModelTypeChangeProcessor::TrackedCacheGuid() const {
  return other_->TrackedCacheGuid();
}

void ForwardingModelTypeChangeProcessor::ReportError(const ModelError& error) {
  other_->ReportError(error);
}

absl::optional<ModelError> ForwardingModelTypeChangeProcessor::GetError()
    const {
  return other_->GetError();
}

base::WeakPtr<ModelTypeControllerDelegate>
ForwardingModelTypeChangeProcessor::GetControllerDelegate() {
  return other_->GetControllerDelegate();
}

const sync_pb::EntitySpecifics&
ForwardingModelTypeChangeProcessor::GetPossiblyTrimmedRemoteSpecifics(
    const std::string& storage_key) const {
  return other_->GetPossiblyTrimmedRemoteSpecifics(storage_key);
}

base::WeakPtr<ModelTypeChangeProcessor>
ForwardingModelTypeChangeProcessor::GetWeakPtr() {
  // Note: Don't bother with a separate WeakPtrFactory for the forwarding
  // processor; just hand out a WeakPtr directly to the real processor.
  return other_->GetWeakPtr();
}

}  // namespace syncer
