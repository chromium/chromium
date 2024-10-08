// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"

#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/sync/base/data_type.h"

namespace tab_groups {

SyncBridgeTabGroupModelWrapper::SyncBridgeTabGroupModelWrapper(
    syncer::DataType sync_data_type,
    SavedTabGroupModel* model,
    LoadCallback on_load_callback)
    : sync_data_type_(sync_data_type),
      model_(model),
      on_load_callback_(std::move(on_load_callback)) {
  CHECK(model_);
  CHECK(on_load_callback_);
}

SyncBridgeTabGroupModelWrapper::~SyncBridgeTabGroupModelWrapper() = default;

bool SyncBridgeTabGroupModelWrapper::IsInitialized() const {
  return model_->is_loaded();
}

std::vector<const SavedTabGroup*> SyncBridgeTabGroupModelWrapper::GetTabGroups()
    const {
  if (IsSharedTabGroupData()) {
    return model_->GetSharedTabGroupsOnly();
  }
  return model_->GetSavedTabGroupsOnly();
}

const SavedTabGroup* SyncBridgeTabGroupModelWrapper::GetGroup(
    const base::Uuid& group_id) const {
  const SavedTabGroup* group = model_->Get(group_id);

  // The `group` must correspond to the same expected data type.
  if (!group || IsSharedTabGroupData() != group->is_shared_tab_group()) {
    return nullptr;
  }
  return group;
}

const SavedTabGroup* SyncBridgeTabGroupModelWrapper::GetGroupContainingTab(
    const base::Uuid& tab_id) const {
  const SavedTabGroup* group = model_->GetGroupContainingTab(tab_id);

  // The `group` must correspond to the same expected data type.
  if (!group || IsSharedTabGroupData() != group->is_shared_tab_group()) {
    return nullptr;
  }
  return group;
}

void SyncBridgeTabGroupModelWrapper::RemoveTabFromGroup(
    const base::Uuid& group_id,
    const base::Uuid& tab_id) {
  // Verify that the group corresponds to the data type.
  const SavedTabGroup* group = model_->Get(group_id);
  CHECK(group);
  CHECK_EQ(group->is_shared_tab_group(), IsSharedTabGroupData());

  model_->RemoveTabFromGroupFromSync(group_id, tab_id);
}

void SyncBridgeTabGroupModelWrapper::RemoveGroup(const base::Uuid& group_id) {
  // Verify that the group corresponds to the data type.
  const SavedTabGroup* group = model_->Get(group_id);
  CHECK(group);
  CHECK_EQ(group->is_shared_tab_group(), IsSharedTabGroupData());

  model_->RemovedFromSync(group_id);
}

const SavedTabGroup* SyncBridgeTabGroupModelWrapper::MergeRemoteGroupMetadata(
    const base::Uuid& group_id,
    const std::u16string& title,
    TabGroupColorId color,
    std::optional<size_t> position,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    base::Time update_time) {
  // Verify that the group corresponds to the data type.
  const SavedTabGroup* group = model_->Get(group_id);
  CHECK(group);
  CHECK_EQ(group->is_shared_tab_group(), IsSharedTabGroupData());

  return model_->MergeRemoteGroupMetadata(
      group_id, title, color, position, std::move(creator_cache_guid),
      std::move(last_updater_cache_guid), update_time);
}

const SavedTabGroupTab* SyncBridgeTabGroupModelWrapper::MergeRemoteTab(
    const SavedTabGroupTab& remote_tab) {
  return model_->MergeRemoteTab(remote_tab);
}

void SyncBridgeTabGroupModelWrapper::AddGroup(SavedTabGroup group) {
  CHECK_EQ(group.is_shared_tab_group(), IsSharedTabGroupData());

  model_->AddedFromSync(std::move(group));
}

void SyncBridgeTabGroupModelWrapper::AddTabToGroup(const base::Uuid& group_id,
                                                   SavedTabGroupTab tab) {
  // Verify that the group corresponds to the data type.
  const SavedTabGroup* group = model_->Get(group_id);
  CHECK(group);
  CHECK_EQ(group->is_shared_tab_group(), IsSharedTabGroupData());

  model_->AddTabToGroupFromSync(group_id, std::move(tab));
}

std::pair<std::set<base::Uuid>, std::set<base::Uuid>>
SyncBridgeTabGroupModelWrapper::UpdateLocalCacheGuid(
    std::optional<std::string> old_cache_guid,
    std::optional<std::string> new_cache_guid) {
  return model_->UpdateLocalCacheGuid(std::move(old_cache_guid),
                                      std::move(new_cache_guid));
}

void SyncBridgeTabGroupModelWrapper::Initialize(
    std::vector<SavedTabGroup> groups,
    std::vector<SavedTabGroupTab> tabs) {
  std::move(on_load_callback_).Run(std::move(groups), std::move(tabs));
}

bool SyncBridgeTabGroupModelWrapper::IsSharedTabGroupData() const {
  return sync_data_type_ == syncer::SHARED_TAB_GROUP_DATA;
}

}  // namespace tab_groups
