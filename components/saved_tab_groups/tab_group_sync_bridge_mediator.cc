// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_bridge_mediator.h"

#include <iterator>
#include <memory>

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"

namespace tab_groups {

TabGroupSyncBridgeMediator::TabGroupSyncBridgeMediator(
    SavedTabGroupModel* model,
    PrefService* pref_service,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration)
    : model_(model) {
  CHECK(model_);
  CHECK(saved_tab_group_configuration);
  // `shared_tab_group_configuration` can be null when the feature is disabled.

  // It is safe to use base::Unretained() because current object outlives the
  // bridges.
  saved_bridge_ = std::make_unique<SavedTabGroupSyncBridge>(
      model_,
      std::move(saved_tab_group_configuration->model_type_store_factory),
      std::move(saved_tab_group_configuration->change_processor), pref_service,
      base::BindOnce(&TabGroupSyncBridgeMediator::OnSavedGroupsWithTabsLoaded,
                     base::Unretained(this)));
  if (shared_tab_group_configuration) {
    shared_bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        model_,
        std::move(shared_tab_group_configuration->model_type_store_factory),
        std::move(shared_tab_group_configuration->change_processor),
        pref_service,
        base::BindOnce(
            &TabGroupSyncBridgeMediator::OnSharedGroupsWithTabsLoaded,
            base::Unretained(this)));
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
  model_->LoadStoredEntries(std::move(loaded_groups_), std::move(loaded_tabs_));
  loaded_groups_.clear();
  loaded_tabs_.clear();

  observation_.Observe(model_);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupSyncBridgeMediator::GetSavedTabGroupControllerDelegate() {
  return saved_bridge_->change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
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

void TabGroupSyncBridgeMediator::SavedTabGroupTabsReorderedLocally(
    const base::Uuid& group_guid) {
  const SavedTabGroup* group = model_->Get(group_guid);
  if (!group) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    // TODO(crbug.com/351357559): support handling positions.
  } else {
    CHECK(saved_bridge_);
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
  if (!group) {
    return;
  }

  if (group->is_shared_tab_group()) {
    CHECK(shared_bridge_);
    // TODO(crbug.com/351357559): support handling local id for shared tab
    // groups.
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
    // TODO(crbug.com/351357559): support handling last user interaction time
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
  saved_tab_groups_loaded_ = true;
  AddGroupsWithTabsImpl(std::move(groups), std::move(tabs));
}

void TabGroupSyncBridgeMediator::OnSharedGroupsWithTabsLoaded(
    std::vector<SavedTabGroup> groups,
    std::vector<SavedTabGroupTab> tabs) {
  CHECK(shared_bridge_);
  CHECK(!shared_tab_groups_loaded_);
  shared_tab_groups_loaded_ = true;
  AddGroupsWithTabsImpl(std::move(groups), std::move(tabs));
}

void TabGroupSyncBridgeMediator::AddGroupsWithTabsImpl(
    std::vector<SavedTabGroup> groups,
    std::vector<SavedTabGroupTab> tabs) {
  loaded_groups_.insert(loaded_groups_.end(),
                        std::make_move_iterator(groups.begin()),
                        std::make_move_iterator(groups.end()));
  loaded_tabs_.insert(loaded_tabs_.end(), std::make_move_iterator(tabs.begin()),
                      std::make_move_iterator(tabs.end()));
  InitializeModelIfReady();
}

}  // namespace tab_groups
