// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_base.h"

#include "components/sync/model/in_memory_metadata_change_list.h"

namespace syncer {

// static
std::unique_ptr<MetadataChangeList>
DataTypeStoreBase::WriteBatch::CreateMetadataChangeList() {
  return std::make_unique<InMemoryMetadataChangeList>();
}

DataTypeStoreBase::WriteBatch::WriteBatch() = default;

DataTypeStoreBase::WriteBatch::~WriteBatch() = default;

void DataTypeStoreBase::WriteBatch::TakeMetadataChangesFrom(
    std::unique_ptr<MetadataChangeList> mcl) {
  static_cast<InMemoryMetadataChangeList*>(mcl.get())->TransferChangesTo(
      GetMetadataChangeList());
}

DataTypeStoreBase::DataTypeStoreBase() = default;

DataTypeStoreBase::~DataTypeStoreBase() = default;

}  // namespace syncer
