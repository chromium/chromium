// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"

#include <iterator>
#include <memory>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/sync/base/data_type.h"

namespace tab_groups {

TabGroupSyncBridgeMediator::TabGroupSyncBridgeMediator(
    SavedTabGroupModel* model,
    PrefService* pref_service,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration)
    : model_(model),
      saved_bridge_model_wrapper_(
          syncer::SAVED_TAB_GROUP,
          model,
          base::BindOnce(
              &TabGroupSyncBridgeMediator::OnSavedGroupsWithTabsLoaded,
              base::Unretained(this))),
      shared_bridge_model_wrapper_(
          syncer::SHARED_TAB_GROUP_DATA,
          model,
          base::BindOnce(
              &TabGroupSyncBridgeMediator::OnSharedGroupsWithTabsLoaded,
              base::Unretained(this))) {
  CHECK(model_);
  CHECK(saved_tab_group_configuration);
  // `shared_tab_group_configuration` can be null when the feature is disabled.

  // It is safe to use base::Unretained() because current object outlives the
  // bridges.
  saved_bridge_ = std::make_unique<SavedTabGroupSyncBridge>(
      &saved_bridge_model_wrapper_,
      std::move(saved_tab_group_configuration->data_type_store_factory),
      std::move(saved_tab_group_configuration->change_processor), pref_service);
  if (shared_tab_group_configuration) {
    shared_bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        &shared_bridge_model_wrapper_,
        std::move(shared_tab_group_configuration->data_type_store_factory),
        std::move(shared_tab_group_configuration->change_processor),
        pref_service);
  }
}

TabGroupSyncBridgeMediator::~TabGroupSyncBridgeMediator() = default;

void TabGroupSyncBridgeMediator::InitializeModelIfReady() {
  if (!saved_tab_groups_loaded_) {
    return;
  }
  if (shared_bridge_ && !shared_tab_groups_loaded_) {
    // Wait for the shared tab group data only if the feature is enabled (i.e.
    // the bridge exists).
    return;
  }

  // There are no duplicate GUIDs in groups and tabs of the same type because
  // they are stored with GUID as a storage key. However, there can be duplicate
  // GUIDs across different types. In case of groups, prefer the shared group.
  // Note that tabs from different group types should be handled carefully in
  // this case to avoid exposing saved group to a shared group.
  std::unordered_set<base::Uuid, base::UuidHash> shared_group_guids;
  for (const SavedTabGroup& shared_group : loaded_shared_groups_) {
    shared_group_guids.emplace(shared_group.saved_guid());
  }
  std::unordered_set<base::Uuid, base::UuidHash> shared_tab_guids;
  for (const SavedTabGroupTab& shared_tab : loaded_shared_tabs_) {
    shared_tab_guids.emplace(shared_tab.saved_tab_guid());
  }
  std::vector<SavedTabGroup> all_groups = std::move(loaded_shared_groups_);
  std::vector<SavedTabGroupTab> all_tabs = std::move(loaded_shared_tabs_);
  loaded_shared_groups_.clear();
  loaded_shared_tabs_.clear();

  // Add saved tab groups which don't have a duplicate shared tab group.
  for (SavedTabGroup& saved_group : loaded_saved_groups_) {
    if (shared_group_guids.contains(saved_group.saved_guid())) {
      DVLOG(1) << "Ignore duplicate saved tab group: "
               << saved_group.saved_guid();
      continue;
    }
    all_groups.push_back(std::move(saved_group));
  }
  loaded_saved_groups_.clear();

  // Add saved tabs with parent groups which don't have a duplicate shared tab
  // group, to avoid exposing saved tabs into shared tab group.
  for (SavedTabGroupTab& saved_tab : loaded_saved_tabs_) {
    if (shared_group_guids.contains(saved_tab.saved_group_guid())) {
      DVLOG(1)
          << "Ignore saved tab with parent having duplicate shared tab group";
      continue;
    }
    if (shared_tab_guids.contains(saved_tab.saved_tab_guid())) {
      // The model expects tabs to be unique, prefer the shared tab having the
      // same GUID and ignore the saved tab. Note that normally this should
      // never happen.
      DVLOG(1) << "Ignore duplicate saved tab: " << saved_tab.saved_tab_guid();
      continue;
    }
    all_tabs.push_back(std::move(saved_tab));
  }
  loaded_saved_tabs_.clear();

  model_->LoadStoredEntries(std::move(all_groups), std::move(all_tabs));
  observation_.Observe(model_);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncBridgeMediator::GetSavedTabGroupControllerDelegate() {
  return saved_bridge_->change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncBridgeMediator::GetSharedTabGroupControllerDelegate() {
  CHECK(shared_bridge_);
  return shared_bridge_->change_processor()->GetControllerDelegate();
}

bool TabGroupSyncBridgeMediator::IsSavedBridgeSyncing() const {
  return saved_bridge_->IsSyncing();
}

std::optional<std::string>
TabGroupSyncBridgeMediator::GetLocalCacheGuidForSavedBridge() const {
  return saved_bridge_->GetLocalCacheGuid();
}

std::optional<std::string>
TabGroupSyncBridgeMediator::GetAccountIdForSavedBridge() const {
  return saved_bridge_->GetTrackedAccountId();
}

void TabGroupSyncBridgeMediator::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  const SavedTabGroup* group = model_->Get(guid);
  if (!group) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    shared_bridge_->SavedTabGroupAddedLocally(guid);
  } else {
    CHECK(saved_bridge_);
    saved_bridge_->SavedTabGroupAddedLocally(guid);
  }
}

void TabGroupSyncBridgeMediator::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  if (removed_group.is_shared_tab_group()) {
    CHECK(shared_bridge_);
    shared_bridge_->SavedTabGroupRemovedLocally(removed_group);
  } else {
    CHECK(saved_bridge_);
    saved_bridge_->SavedTabGroupRemovedLocally(removed_group);
  }
}

void TabGroupSyncBridgeMediator::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* group = model_->Get(group_guid);
  if (!group) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    shared_bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  } else {
    CHECK(saved_bridge_);
    saved_bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  }
}

void TabGroupSyncBridgeMediator::SavedTabGroupTabMovedLocally(
    const base::Uuid& group_guid,
    const base::Uuid& tab_guid) {
  const SavedTabGroup* group = model_->Get(group_guid);
  if (!group) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    // Shared tab groups may handle individual tab moves because it uses
    // relative (unique) positions.
    shared_bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  } else {
    CHECK(saved_bridge_);
    // Positions of the other tabs could also be updated, hence handle
    // reordering of all tabs in the group.
    saved_bridge_->SavedTabGroupTabsReorderedLocally(group_guid);
  }
}

void TabGroupSyncBridgeMediator::SavedTabGroupReorderedLocally() {
  CHECK(saved_bridge_);
  saved_bridge_->SavedTabGroupReorderedLocally();

  // Shared tab groups do not handle group reordering.
}

void TabGroupSyncBridgeMediator::SavedTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  const SavedTabGroup* group = model_->Get(group_guid);
  // For desktop, the local ID isn't persisted across sessions. Hence there is
  // no need to rewrite the group to the storage. In fact, it will lead to write
  // inconsistency since we haven't yet fixed the potential reentrancy issue on
  // desktop.
  if (!group || !AreLocalIdsPersisted()) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    shared_bridge_->ProcessTabGroupLocalIdChanged(group_guid);
  } else {
    CHECK(saved_bridge_);
    saved_bridge_->SavedTabGroupLocalIdChanged(group_guid);
  }
}

void TabGroupSyncBridgeMediator::SavedTabGroupLastUserInteractionTimeUpdated(
    const base::Uuid& group_guid) {
  const SavedTabGroup* group = model_->Get(group_guid);
  if (!group) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    // TODO(crbug.com/370710496): support handling last user interaction time
    // for shared tab groups.
  } else {
    CHECK(saved_bridge_);
    saved_bridge_->SavedTabGroupLastUserInteractionTimeUpdated(group_guid);
  }
}

void TabGroupSyncBridgeMediator::OnSavedGroupsWithTabsLoaded(
    std::vector<SavedTabGroup> groups,
    std::vector<SavedTabGroupTab> tabs) {
  CHECK(!saved_tab_groups_loaded_);
  loaded_saved_groups_ = std::move(groups);
  loaded_saved_tabs_ = std::move(tabs);
  saved_tab_groups_loaded_ = true;
  InitializeModelIfReady();
}

void TabGroupSyncBridgeMediator::OnSharedGroupsWithTabsLoaded(
    std::vector<SavedTabGroup> groups,
    std::vector<SavedTabGroupTab> tabs) {
  CHECK(shared_bridge_);
  CHECK(!shared_tab_groups_loaded_);
  loaded_shared_groups_ = std::move(groups);
  loaded_shared_tabs_ = std::move(tabs);
  shared_tab_groups_loaded_ = true;
  InitializeModelIfReady();
}

}  // namespace tab_groups
