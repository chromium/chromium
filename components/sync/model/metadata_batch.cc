// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/metadata_batch.h"

#include <memory>
#include <utility>

#include "components/sync/protocol/entity_metadata.pb.h"

namespace syncer {

MetadataBatch::MetadataBatch() = default;

MetadataBatch::MetadataBatch(MetadataBatch&& other) = default;

MetadataBatch::~MetadataBatch() = default;

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

const sync_pb::DataTypeState& MetadataBatch::GetDataTypeState() const {
  return state_;
}

void MetadataBatch::SetDataTypeState(const sync_pb::DataTypeState& state) {
  state_ = state;
}

}  // namespace syncer
