// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_metadata_store_change_list.h"

#include <utility>

#include "base/location.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace syncer {

SyncMetadataStoreChangeList::SyncMetadataStoreChangeList(
    SyncMetadataStore* store,
    syncer::DataType type,
    ErrorCallback error_callback)
    : store_(store), type_(type), error_callback_(std::move(error_callback)) {
  if (!store_) {
    SetError(ModelError(FROM_HERE, "Invalid SyncMetadataStore"));
  }
}

SyncMetadataStoreChangeList::~SyncMetadataStoreChangeList() = default;

void SyncMetadataStoreChangeList::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {
  if (error_encountered_) {
    return;
  }

  if (!store_->UpdateDataTypeState(type_, data_type_state)) {
    SetError(ModelError(FROM_HERE, "Failed to update DataTypeState."));
  }
}

void SyncMetadataStoreChangeList::ClearDataTypeState() {
  if (error_encountered_) {
    return;
  }

  if (!store_->ClearDataTypeState(type_)) {
    SetError(ModelError(FROM_HERE, "Failed to clear DataTypeState."));
  }
}

void SyncMetadataStoreChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  if (error_encountered_) {
    return;
  }

  if (!store_->UpdateEntityMetadata(type_, storage_key, metadata)) {
    SetError(ModelError(FROM_HERE, "Failed to update entity metadata."));
  }
}

void SyncMetadataStoreChangeList::ClearMetadata(
    const std::string& storage_key) {
  if (error_encountered_) {
    return;
  }

  if (!store_->ClearEntityMetadata(type_, storage_key)) {
    SetError(ModelError(FROM_HERE, "Failed to clear entity metadata."));
  }
}

const SyncMetadataStore*
SyncMetadataStoreChangeList::GetMetadataStoreForTesting() const {
  return store_;
}

void SyncMetadataStoreChangeList::SetError(ModelError error) {
  if (!error_encountered_) {
    error_encountered_ = true;
    error_callback_.Run(error);
  }
}

}  // namespace syncer
