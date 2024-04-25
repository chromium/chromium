// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_service_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace tab_groups {
TabGroupSyncServiceImpl::SyncDataTypeConfiguration::SyncDataTypeConfiguration(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> processor,
    syncer::OnceModelTypeStoreFactory store_factory)
    : change_processor(std::move(processor)),
      model_type_store_factory(std::move(store_factory)) {}

TabGroupSyncServiceImpl::SyncDataTypeConfiguration::
    ~SyncDataTypeConfiguration() = default;

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration)
    : model_(std::move(model)),
      saved_bridge_(
          model_.get(),
          std::move(saved_tab_group_configuration->model_type_store_factory),
          std::move(saved_tab_group_configuration->change_processor)) {
  if (shared_tab_group_configuration) {
    shared_bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        model_.get(),
        std::move(shared_tab_group_configuration->model_type_store_factory),
        std::move(shared_tab_group_configuration->change_processor));
  }
  model_->AddObserver(this);
}

TabGroupSyncServiceImpl::~TabGroupSyncServiceImpl() {
  model_->RemoveObserver(this);
}

void TabGroupSyncServiceImpl::AddObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.AddObserver(observer);

  // If the observer is added late and missed the init signal, send the signal
  // now.
  if (model_->is_loaded()) {
    observer->OnInitialized();
  }
}

void TabGroupSyncServiceImpl::RemoveObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSavedTabGroupControllerDelegate() {
  return saved_bridge_.change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupControllerDelegate() {
  if (!shared_bridge_) {
    return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
  }

  return shared_bridge_->change_processor()->GetControllerDelegate();
}

void TabGroupSyncServiceImpl::AddGroup(const SavedTabGroup& group) {
  model_->Add(group);
  // TODO(b/336865528): Add to mapping store.
}

void TabGroupSyncServiceImpl::RemoveGroup(const LocalTabGroupID& local_id) {
  model_->Remove(local_id);
  // TODO(b/336865528): Remove from mapping store.
}

void TabGroupSyncServiceImpl::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  model_->UpdateVisualData(local_group_id, visual_data);
}

void TabGroupSyncServiceImpl::AddTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     const std::u16string& title,
                                     GURL url,
                                     std::optional<size_t> position) {
  auto* group = model_->Get(group_id);
  if (!group) {
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (tab) {
    return;
  }

  SavedTabGroupTab new_tab(url, title, group->saved_guid(), position,
                           /*saved_tab_guid=*/std::nullopt, tab_id);
  model_->AddTabToGroupLocally(group->saved_guid(), new_tab);
}

void TabGroupSyncServiceImpl::UpdateTab(const LocalTabGroupID& group_id,
                                        const LocalTabID& tab_id,
                                        const std::u16string& title,
                                        GURL url,
                                        std::optional<size_t> position) {
  auto* group = model_->Get(group_id);
  if (!group) {
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  SavedTabGroupTab updated_tab(*tab);
  updated_tab.SetLocalTabID(tab_id);
  updated_tab.SetTitle(title);
  updated_tab.SetURL(url);
  if (position.has_value()) {
    updated_tab.SetPosition(position.value());
  }
  model_->UpdateTabInGroup(group->saved_guid(), updated_tab);
}

void TabGroupSyncServiceImpl::RemoveTab(const LocalTabGroupID& group_id,
                                        const LocalTabID& tab_id) {
  auto* group = model_->Get(group_id);
  if (!group) {
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  // TODO(b/336865528): If it's the last tab, we will remove the group from
  // model. We should delete from mapping store too.
  model_->RemoveTabFromGroupLocally(group->saved_guid(), tab->saved_tab_guid());
}

std::vector<SavedTabGroup> TabGroupSyncServiceImpl::GetAllGroups() {
  return model_->saved_tab_groups();
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const base::Uuid& guid) {
  const SavedTabGroup* tab_group = model_->Get(guid);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    LocalTabGroupID& local_id) {
  const SavedTabGroup* tab_group = model_->Get(local_id);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceImpl::GetDeletedGroupIds() {
  // TODO(b/336865528): Query mapping store.
  return std::vector<LocalTabGroupID>();
}

void TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  model_->OnGroupOpenedInTabStrip(sync_id, local_id);
  // TODO(b/336865528): Update mapping store.
}

void TabGroupSyncServiceImpl::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id) {
  model_->OnGroupClosedInTabStrip(local_id);
  // TODO(b/336865528): Delete from mapping store.
}

void TabGroupSyncServiceImpl::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  auto* group = model_->Get(local_group_id);
  CHECK(group);

  const auto* tab = group->GetTab(sync_tab_id);
  CHECK(tab);

  model_->UpdateLocalTabId(group->saved_guid(), *tab, local_tab_id);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  CHECK(saved_tab_group);
  if (saved_tab_group->saved_tabs().empty()) {
    // Wait for another sync update with tabs before notifying the UI.
    empty_groups_.emplace(guid);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(*saved_tab_group, TriggerSource::REMOTE);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  CHECK(saved_tab_group);

  if (saved_tab_group->saved_tabs().empty()) {
    return;
  }

  if (base::Contains(empty_groups_, group_guid)) {
    empty_groups_.erase(group_guid);
    SavedTabGroupAddedFromSync(group_guid);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, TriggerSource::REMOTE);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group->saved_guid());
  }

  auto local_id = removed_group->local_group_id();
  if (!local_id.has_value()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(local_id.value());
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupModelLoaded() {
  // TODO(b/336865528): Initialize and query mapping store before broadcasting.
  for (auto& observer : observers_) {
    observer.OnInitialized();
  }
}

}  // namespace tab_groups
