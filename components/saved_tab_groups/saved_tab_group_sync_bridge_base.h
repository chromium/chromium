// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_BASE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_BASE_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"

namespace syncer {
class ModelTypeChangeProcessor;
class MetadataChangeList;
}  // namespace syncer

// Serves as the virtual interface for SavedTabGroupSyncBridge.
class SavedTabGroupSyncBridgeBase : public syncer::ModelTypeSyncBridge {
 public:
  explicit SavedTabGroupSyncBridgeBase(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  SavedTabGroupSyncBridgeBase(const SavedTabGroupSyncBridgeBase&) = delete;
  SavedTabGroupSyncBridgeBase& operator=(const SavedTabGroupSyncBridgeBase&) =
      delete;

  ~SavedTabGroupSyncBridgeBase() override;

  // Add/Update a SavedTabGroupSpecifics to sync.
  virtual void UpsertEntitySpecific(
      const sync_pb::SavedTabGroupSpecifics& specific) = 0;

  // Remove a SavedTabGroupSpecifics from sync using the guid. If removing a
  // group, all tabs tied to that group will also be removed.
  virtual void RemoveEntitySpecific(const std::string& guid) = 0;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_BASE_H_
