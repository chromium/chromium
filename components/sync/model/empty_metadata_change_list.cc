// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/empty_metadata_change_list.h"

namespace syncer {

EmptyMetadataChangeList::EmptyMetadataChangeList() = default;
EmptyMetadataChangeList::~EmptyMetadataChangeList() = default;

void EmptyMetadataChangeList::UpdateDataTypeState(
    const sync_pb::DataTypeState& data_type_state) {}

void EmptyMetadataChangeList::ClearDataTypeState() {}

void EmptyMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {}

void EmptyMetadataChangeList::ClearMetadata(const std::string& storage_key) {}

}  // namespace syncer
