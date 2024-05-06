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
#include "components/saved_tab_groups/tab_group_store.h"
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
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
    std::unique_ptr<TabGroupStore> tab_group_store)
    : model_(std::move(model)),
      saved_bridge_(
          model_.get(),
          std::move(saved_tab_group_configuration->model_type_store_factory),
          std::move(saved_tab_group_configuration->change_processor)),
      tab_group_store_(std::move(tab_group_store)) {
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
  if (is_initialized_) {
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
  VLOG(2) << __func__;
  model_->Add(group);
  tab_group_store_->StoreTabGroupIDMetadata(
      group.saved_guid(), TabGroupIDMetadata(group.local_group_id().value()));
}

void TabGroupSyncServiceImpl::RemoveGroup(const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  model_->Remove(local_id);
  tab_group_store_->DeleteTabGroupIDMetadata(sync_id);
}

void TabGroupSyncServiceImpl::RemoveGroup(const base::Uuid& sync_id) {
  VLOG(2) << __func__;
  model_->Remove(sync_id);
  tab_group_store_->DeleteTabGroupIDMetadata(sync_id);
}

void TabGroupSyncServiceImpl::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  VLOG(2) << __func__;
  model_->UpdateVisualData(local_group_id, visual_data);
}

void TabGroupSyncServiceImpl::AddTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     const std::u16string& title,
                                     GURL url,
                                     std::optional<size_t> position) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    VLOG(2) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (tab) {
    VLOG(2) << __func__ << " Called for a tab that already exists";
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
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    VLOG(2) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    VLOG(2) << __func__ << " Called for a tab that doesn't exist";
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
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  model_->RemoveTabFromGroupLocally(sync_id, tab->saved_tab_guid());

  // The group might have deleted if this was the last tab, hence we should
  // delete it from mapping store too.
  group = model_->Get(group_id);
  if (!group) {
    tab_group_store_->DeleteTabGroupIDMetadata(sync_id);
  }
}

void TabGroupSyncServiceImpl::MoveTab(const LocalTabGroupID& group_id,
                                      const LocalTabID& tab_id,
                                      int new_group_index) {
  auto* group = model_->Get(group_id);
  if (!group) {
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  model_->MoveTabInGroupTo(group->saved_guid(), tab->saved_tab_guid(),
                           new_group_index);
}

std::vector<SavedTabGroup> TabGroupSyncServiceImpl::GetAllGroups() {
  VLOG(2) << __func__;
  return model_->saved_tab_groups();
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const base::Uuid& guid) {
  VLOG(2) << __func__;
  const SavedTabGroup* tab_group = model_->Get(guid);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    LocalTabGroupID& local_id) {
  const SavedTabGroup* tab_group = model_->Get(local_id);
  VLOG(2) << __func__;
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceImpl::GetDeletedGroupIds() {
  std::vector<LocalTabGroupID> deleted_ids;

  // Deleted groups are groups that have been deleted from sync, but we haven't
  // deleted them from mapping, since the local tab group still exists.
  std::set<base::Uuid> ids_from_sync;
  for (const auto& group : GetAllGroups()) {
    ids_from_sync.insert(group.saved_guid());
  }

  for (const auto& pair : tab_group_store_->GetAllTabGroupIDMetadata()) {
    const base::Uuid& id = pair.first;
    if (base::Contains(ids_from_sync, id)) {
      continue;
    }

    // Model doesn't know about this entry. Hence this is a deleted entry.
    deleted_ids.emplace_back(pair.second.local_tab_group_id);
  }

  return deleted_ids;
}

void TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;
  model_->OnGroupOpenedInTabStrip(sync_id, local_id);
  tab_group_store_->StoreTabGroupIDMetadata(sync_id,
                                            TabGroupIDMetadata(local_id));
}

void TabGroupSyncServiceImpl::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;
  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  model_->OnGroupClosedInTabStrip(local_id);
  tab_group_store_->DeleteTabGroupIDMetadata(group->saved_guid());
}

void TabGroupSyncServiceImpl::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  VLOG(2) << __func__;
  auto* group = model_->Get(local_group_id);
  CHECK(group);

  const auto* tab = group->GetTab(sync_tab_id);
  CHECK(tab);

  model_->UpdateLocalTabId(group->saved_guid(), *tab, local_tab_id);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  HandleTabGroupUpdated(group_guid, tab_guid, TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  HandleTabGroupUpdated(group_guid, tab_guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  HandleTabGroupRemoved(removed_group, TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedLocally(
    const SavedTabGroup* removed_group) {
  HandleTabGroupRemoved(removed_group, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::HandleTabGroupAdded(const base::Uuid& guid,
                                                  TriggerSource source) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  CHECK(saved_tab_group);
  if (saved_tab_group->saved_tabs().empty()) {
    // Wait for another sync update with tabs before notifying the UI.
    empty_groups_.emplace(guid);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupUpdated(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid,
    TriggerSource source) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  CHECK(saved_tab_group);

  if (saved_tab_group->saved_tabs().empty()) {
    return;
  }

  if (base::Contains(empty_groups_, group_guid)) {
    empty_groups_.erase(group_guid);
    HandleTabGroupAdded(group_guid, source);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupRemoved(
    const SavedTabGroup* removed_group,
    TriggerSource source) {
  VLOG(2) << __func__;
  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group->saved_guid(), source);
  }

  auto local_id = removed_group->local_group_id();
  if (!local_id.has_value()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(local_id.value(), source);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  CHECK(saved_tab_group);
  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupModelLoaded() {
  VLOG(2) << __func__;

  tab_group_store_->Initialize(
      base::BindOnce(&TabGroupSyncServiceImpl::OnReadTabGroupStore,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupSyncServiceImpl::OnReadTabGroupStore() {
  VLOG(2) << __func__;

  for (const auto& group : GetAllGroups()) {
    auto sync_id = group.saved_guid();
    auto id_metadata = tab_group_store_->GetTabGroupIDMetadata(sync_id);
    if (!id_metadata) {
      continue;
    }

    model_->OnGroupOpenedInTabStrip(sync_id, id_metadata->local_tab_group_id);
  }

  is_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnInitialized();
  }
}

}  // namespace tab_groups
