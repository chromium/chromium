// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/recording_model_type_change_processor.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/fake_model_type_sync_bridge.h"
#include "components/sync/model/metadata_batch.h"

namespace syncer {

RecordingModelTypeChangeProcessor::RecordingModelTypeChangeProcessor() {}

RecordingModelTypeChangeProcessor::~RecordingModelTypeChangeProcessor() {}

void RecordingModelTypeChangeProcessor::Put(
    const std::string& storage_key,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_changes) {
  put_multimap_.insert(std::make_pair(storage_key, std::move(entity_data)));
}

void RecordingModelTypeChangeProcessor::Delete(
    const std::string& storage_key,
    MetadataChangeList* metadata_changes) {
  delete_set_.insert(storage_key);
}

void RecordingModelTypeChangeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  update_multimap_.insert(std::make_pair(
      storage_key, FakeModelTypeSyncBridge::CopyEntityData(entity_data)));
}

void RecordingModelTypeChangeProcessor::UntrackEntity(
    const EntityData& entity_data) {
  untrack_set_.insert(FakeModelTypeSyncBridge::CopyEntityData(entity_data));
}

void RecordingModelTypeChangeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  untrack_for_storage_key_set_.insert(storage_key);
}

void RecordingModelTypeChangeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {
  std::swap(metadata_, batch);
}

bool RecordingModelTypeChangeProcessor::IsTrackingMetadata() {
  return is_tracking_metadata_;
}

std::string RecordingModelTypeChangeProcessor::TrackedAccountId() {
  return "";
}

void RecordingModelTypeChangeProcessor::SetIsTrackingMetadata(
    bool is_tracking) {
  is_tracking_metadata_ = is_tracking;
}

// static
std::unique_ptr<ModelTypeChangeProcessor>
RecordingModelTypeChangeProcessor::CreateProcessorAndAssignRawPointer(
    RecordingModelTypeChangeProcessor** processor_address) {
  auto processor = std::make_unique<RecordingModelTypeChangeProcessor>();
  *processor_address = processor.get();
  // Not all compilers are smart enough to up cast during copy elision, so we
  // explicitly create a correctly typed unique_ptr.
  return base::WrapUnique(processor.release());
}

}  //  namespace syncer
