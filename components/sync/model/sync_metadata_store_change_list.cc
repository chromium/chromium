// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_metadata_store_change_list.h"

#include <utility>

#include "base/location.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

SyncMetadataStoreChangeList::SyncMetadataStoreChangeList(
    SyncMetadataStore* store,
    syncer::ModelType type,
    ErrorCallback error_callback)
    : store_(store), type_(type), error_callback_(std::move(error_callback)) {
  if (!store_) {
    SetError(ModelError(FROM_HERE, "Invalid SyncMetadataStore"));
  }
}

SyncMetadataStoreChangeList::~SyncMetadataStoreChangeList() = default;

void SyncMetadataStoreChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  if (error_) {
    return;
  }

  if (!store_->UpdateModelTypeState(type_, model_type_state)) {
    SetError(ModelError(FROM_HERE, "Failed to update ModelTypeState."));
  }
}

void SyncMetadataStoreChangeList::ClearModelTypeState() {
  if (error_) {
    return;
  }

  if (!store_->ClearModelTypeState(type_)) {
    SetError(ModelError(FROM_HERE, "Failed to clear ModelTypeState."));
  }
}

void SyncMetadataStoreChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  if (error_) {
    return;
  }

  if (!store_->UpdateEntityMetadata(type_, storage_key, metadata)) {
    SetError(ModelError(FROM_HERE, "Failed to update entity metadata."));
  }
}

void SyncMetadataStoreChangeList::ClearMetadata(
    const std::string& storage_key) {
  if (error_) {
    return;
  }

  if (!store_->ClearEntityMetadata(type_, storage_key)) {
    SetError(ModelError(FROM_HERE, "Failed to clear entity metadata."));
  }
}

absl::optional<ModelError> SyncMetadataStoreChangeList::TakeError() {
  absl::optional<ModelError> temp = error_;
  error_.reset();
  return temp;
}

const SyncMetadataStore*
SyncMetadataStoreChangeList::GetMetadataStoreForTesting() const {
  return store_;
}

void SyncMetadataStoreChangeList::SetError(ModelError error) {
  if (!error_) {
    error_ = std::move(error);
    error_callback_.Run(*error_);
  }
}

}  // namespace syncer
