// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/dummy_metadata_change_list.h"

namespace syncer {

DummyMetadataChangeList::DummyMetadataChangeList() = default;
DummyMetadataChangeList::~DummyMetadataChangeList() = default;

void DummyMetadataChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {}

void DummyMetadataChangeList::ClearModelTypeState() {}

void DummyMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {}

void DummyMetadataChangeList::ClearMetadata(const std::string& storage_key) {}

}  // namespace syncer
