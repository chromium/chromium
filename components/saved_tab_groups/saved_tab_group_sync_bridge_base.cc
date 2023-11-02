// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_sync_bridge_base.h"

#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"

SavedTabGroupSyncBridgeBase::SavedTabGroupSyncBridgeBase(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {}

SavedTabGroupSyncBridgeBase::~SavedTabGroupSyncBridgeBase() = default;

std::unique_ptr<syncer::MetadataChangeList>
SavedTabGroupSyncBridgeBase::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}
