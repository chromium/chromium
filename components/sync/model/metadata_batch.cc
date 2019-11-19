// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/metadata_batch.h"

#include <memory>
#include <utility>

namespace syncer {

MetadataBatch::MetadataBatch() {}

MetadataBatch::MetadataBatch(MetadataBatch&& other)
    : metadata_map_(std::move(other.metadata_map_)) {
  other.state_.Swap(&state_);
}

MetadataBatch::~MetadataBatch() {}

const EntityMetadataMap& MetadataBatch::GetAllMetadata() const {
  return metadata_map_;
}

EntityMetadataMap MetadataBatch::TakeAllMetadata() {
  return std::move(metadata_map_);
}

void MetadataBatch::AddMetadata(
    const std::string& storage_key,
    std::unique_ptr<sync_pb::EntityMetadata> metadata) {
  metadata_map_.insert(std::make_pair(storage_key, std::move(metadata)));
}

const sync_pb::ModelTypeState& MetadataBatch::GetModelTypeState() const {
  return state_;
}

void MetadataBatch::SetModelTypeState(const sync_pb::ModelTypeState& state) {
  state_ = state;
}

}  // namespace syncer
