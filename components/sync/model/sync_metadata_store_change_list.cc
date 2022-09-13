// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_metadata_store_change_list.h"

#include "components/sync/protocol/entity_metadata.pb.h"

#include "base/location.h"

using absl::optional;
using syncer::ModelError;

namespace syncer {

SyncMetadataStoreChangeList::SyncMetadataStoreChangeList(
    SyncMetadataStore* store,
    syncer::ModelType type)
    : store_(store), type_(type) {
  if (!store_) {
    error_ = ModelError(FROM_HERE, "Invalid SyncMetadataStore");
  }
}

SyncMetadataStoreChangeList::~SyncMetadataStoreChangeList() {
  DCHECK(!error_);
}

void SyncMetadataStoreChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  if (error_) {
    return;
  }

  if (!store_->UpdateModelTypeState(type_, model_type_state)) {
    error_ = ModelError(FROM_HERE, "Failed to update ModelTypeState.");
  }
}

void SyncMetadataStoreChangeList::ClearModelTypeState() {
  if (error_) {
    return;
  }

  if (!store_->ClearModelTypeState(type_)) {
    error_ = ModelError(FROM_HERE, "Failed to clear ModelTypeState.");
  }
}

void SyncMetadataStoreChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  if (error_) {
    return;
  }

  if (!store_->UpdateSyncMetadata(type_, storage_key, metadata)) {
    error_ = ModelError(FROM_HERE, "Failed to update entity metadata.");
  }
}

void SyncMetadataStoreChangeList::ClearMetadata(
    const std::string& storage_key) {
  if (error_) {
    return;
  }

  if (!store_->ClearSyncMetadata(type_, storage_key)) {
    error_ = ModelError(FROM_HERE, "Failed to clear entity metadata.");
  }
}

optional<ModelError> SyncMetadataStoreChangeList::TakeError() {
  optional<ModelError> temp = error_;
  error_.reset();
  return temp;
}

const SyncMetadataStore*
SyncMetadataStoreChangeList::GetMetadataStoreForTesting() const {
  return store_;
}

}  // namespace syncer
