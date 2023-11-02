// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_base.h"

#include "components/sync/model/in_memory_metadata_change_list.h"

namespace syncer {

// static
std::unique_ptr<MetadataChangeList>
ModelTypeStoreBase::WriteBatch::CreateMetadataChangeList() {
  return std::make_unique<InMemoryMetadataChangeList>();
}

ModelTypeStoreBase::WriteBatch::WriteBatch() = default;

ModelTypeStoreBase::WriteBatch::~WriteBatch() = default;

void ModelTypeStoreBase::WriteBatch::TakeMetadataChangesFrom(
    std::unique_ptr<MetadataChangeList> mcl) {
  static_cast<InMemoryMetadataChangeList*>(mcl.get())->TransferChangesTo(
      GetMetadataChangeList());
}

ModelTypeStoreBase::ModelTypeStoreBase() = default;

ModelTypeStoreBase::~ModelTypeStoreBase() = default;

}  // namespace syncer
