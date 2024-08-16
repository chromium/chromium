// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"

#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"

namespace web_app {

namespace sync_bridge_test_utils {

void AddApps(WebAppSyncBridge& sync_bridge,
             const std::vector<std::unique_ptr<WebApp>>& apps_server_state) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      sync_bridge.CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;

  for (const std::unique_ptr<WebApp>& web_app_server_state :
       apps_server_state) {
    // Only fallback icon infos from SyncFallbackData are used.
    DCHECK(web_app_server_state->manifest_icons().empty());

    std::unique_ptr<syncer::EntityData> entity_data =
        CreateSyncEntityData(*web_app_server_state);

    auto entity_change = syncer::EntityChange::CreateAdd(
        web_app_server_state->app_id(), std::move(*entity_data));
    entity_changes.push_back(std::move(entity_change));
  }

  sync_bridge.ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                          std::move(entity_changes));
}

void UpdateApps(WebAppSyncBridge& sync_bridge,
                const std::vector<std::unique_ptr<WebApp>>& apps_server_state) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      sync_bridge.CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;

  for (const std::unique_ptr<WebApp>& web_app_server_state :
       apps_server_state) {
    std::unique_ptr<syncer::EntityData> entity_data =
        CreateSyncEntityData(*web_app_server_state);

    auto entity_change = syncer::EntityChange::CreateUpdate(
        web_app_server_state->app_id(), std::move(*entity_data));
    entity_changes.push_back(std::move(entity_change));
  }

  sync_bridge.ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                          std::move(entity_changes));
}

void DeleteApps(WebAppSyncBridge& sync_bridge,
                const std::vector<webapps::AppId>& app_ids_to_delete) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      sync_bridge.CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;

  for (const webapps::AppId& app_id : app_ids_to_delete) {
    auto entity_change = syncer::EntityChange::CreateDelete(app_id);
    entity_changes.push_back(std::move(entity_change));
  }

  sync_bridge.ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                          std::move(entity_changes));
}

}  // namespace sync_bridge_test_utils

}  // namespace web_app
