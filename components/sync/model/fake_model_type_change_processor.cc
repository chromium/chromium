// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/fake_model_type_change_processor.h"

#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

FakeModelTypeChangeProcessor::FakeModelTypeChangeProcessor()
    : FakeModelTypeChangeProcessor(nullptr) {}

FakeModelTypeChangeProcessor::FakeModelTypeChangeProcessor(
    base::WeakPtr<ModelTypeControllerDelegate> delegate)
    : delegate_(delegate) {}

FakeModelTypeChangeProcessor::~FakeModelTypeChangeProcessor() {}

void FakeModelTypeChangeProcessor::Put(
    const std::string& client_tag,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::Delete(
    const std::string& client_tag,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {}

void FakeModelTypeChangeProcessor::UntrackEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {}

bool FakeModelTypeChangeProcessor::IsEntityUnsynced(
    const std::string& storage_key) {
  return false;
}

base::Time FakeModelTypeChangeProcessor::GetEntityCreationTime(
    const std::string& storage_key) const {
  return base::Time();
}

base::Time FakeModelTypeChangeProcessor::GetEntityModificationTime(
    const std::string& storage_key) const {
  return base::Time();
}

void FakeModelTypeChangeProcessor::OnModelStarting(
    ModelTypeSyncBridge* bridge) {}

void FakeModelTypeChangeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {}

bool FakeModelTypeChangeProcessor::IsTrackingMetadata() {
  return true;
}

std::string FakeModelTypeChangeProcessor::TrackedAccountId() {
  return "";
}

std::string FakeModelTypeChangeProcessor::TrackedCacheGuid() {
  return "";
}

void FakeModelTypeChangeProcessor::ReportError(const ModelError& error) {
  error_ = error;
}

base::Optional<ModelError> FakeModelTypeChangeProcessor::GetError() const {
  return error_;
}

base::WeakPtr<ModelTypeControllerDelegate>
FakeModelTypeChangeProcessor::GetControllerDelegate() {
  return delegate_;
}

}  // namespace syncer
