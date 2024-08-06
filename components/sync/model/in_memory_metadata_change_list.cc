// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/in_memory_metadata_change_list.h"

#include <memory>

namespace syncer {

InMemoryMetadataChangeList::InMemoryMetadataChangeList() = default;
InMemoryMetadataChangeList::~InMemoryMetadataChangeList() = default;

void InMemoryMetadataChangeList::TransferChangesTo(MetadataChangeList* other) {
  DCHECK(other);
  for (const auto& [storage_key, change] : metadata_changes_) {
    switch (change.type) {
      case UPDATE:
        other->UpdateMetadata(storage_key, change.metadata);
        break;
      case CLEAR:
        other->ClearMetadata(storage_key);
        break;
    }
  }
  metadata_changes_.clear();
  if (state_change_) {
    switch (state_change_->type) {
      case UPDATE:
        other->UpdateDataTypeState(state_change_->state);
        break;
      case CLEAR:
        other->ClearDataTypeState();
        break;
    }
    state_change_.reset();
  }
}

void InMemoryMetadataChangeList::DropMetadataChangeForStorageKey(
    const std::string& storage_key) {
  metadata_changes_.erase(storage_key);
}

void InMemoryMetadataChangeList::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {
  state_change_ = std::make_unique<DataTypeStateChange>(
      DataTypeStateChange{UPDATE, data_type_state});
}

void InMemoryMetadataChangeList::ClearDataTypeState() {
  state_change_ = std::make_unique<DataTypeStateChange>(
      DataTypeStateChange{CLEAR, sync_pb::DataTypeState()});
}

void InMemoryMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  metadata_changes_[storage_key] = {UPDATE, metadata};
}

void InMemoryMetadataChangeList::ClearMetadata(const std::string& storage_key) {
  metadata_changes_[storage_key] = {CLEAR, sync_pb::EntityMetadata()};
}

}  // namespace syncer
