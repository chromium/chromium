// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_service_wrapper.h"

#include <optional>
#include <string>
#include <vector>

#include "base/notimplemented.h"
#include "base/observer_list.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/types.h"

class Profile;

namespace tab_groups {

TabGroupServiceWrapper::TabGroupServiceWrapper(
    TabGroupSyncService* tab_group_sync_service,
    SavedTabGroupKeyedService* saved_tab_group_keyed_service)
    : sync_service_(tab_group_sync_service),
      saved_keyed_service_(saved_tab_group_keyed_service) {}

TabGroupServiceWrapper::~TabGroupServiceWrapper() = default;

void TabGroupServiceWrapper::AddGroup(SavedTabGroup group) {
  if (ShouldUseSyncService()) {
    sync_service_->AddGroup(std::move(group));
  } else {
    saved_keyed_service_->model()->Add(std::move(group));
  }
}

void TabGroupServiceWrapper::RemoveGroup(const LocalTabGroupID& local_id) {
  if (ShouldUseSyncService()) {
    sync_service_->RemoveGroup(local_id);
  } else {
    saved_keyed_service_->model()->Remove(local_id);
  }
}

void TabGroupServiceWrapper::RemoveGroup(const base::Uuid& sync_id) {
  if (ShouldUseSyncService()) {
    sync_service_->RemoveGroup(sync_id);
  } else {
    saved_keyed_service_->model()->Remove(sync_id);
  }
}

void TabGroupServiceWrapper::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const TabGroupVisualData* visual_data) {
  if (ShouldUseSyncService()) {
    sync_service_->UpdateVisualData(local_group_id, visual_data);
  } else {
    saved_keyed_service_->UpdateAttributions(local_group_id);
    saved_keyed_service_->model()->UpdateVisualData(local_group_id,
                                                    visual_data);
  }

  std::optional<SavedTabGroup> group = GetGroup(local_group_id);
  CHECK(group.has_value());
  OnTabGroupVisualsChanged(group->saved_guid());
}

void TabGroupServiceWrapper::AddTab(const LocalTabGroupID& group_id,
                                    const LocalTabID& tab_id,
                                    const std::u16string& title,
                                    GURL url,
                                    std::optional<size_t> position) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());

  if (ShouldUseSyncService()) {
    sync_service_->AddTab(group_id, tab_id, title, url, position);
  } else {
    SavedTabGroupTab new_tab(url, title, group->saved_guid(), position,
                             /*saved_tab_guid=*/std::nullopt, tab_id);
    new_tab.SetLocalTabID(tab_id);
    if (position.has_value()) {
      new_tab.SetPosition(position.value());
    }
    new_tab.SetCreatorCacheGuid(saved_keyed_service_->GetLocalCacheGuid());
    saved_keyed_service_->UpdateAttributions(group_id);
    saved_keyed_service_->model()->AddTabToGroupLocally(group->saved_guid(),
                                                        new_tab);
  }

  OnTabAddedToGroupLocally(group->saved_guid());
}

void TabGroupServiceWrapper::UpdateTab(const LocalTabGroupID& group_id,
                                       const LocalTabID& tab_id,
                                       const std::u16string& title,
                                       GURL url,
                                       std::optional<size_t> position) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  if (ShouldUseSyncService()) {
    sync_service_->UpdateTab(group_id, tab_id, title, url, position);
  } else {
    saved_keyed_service_->UpdateAttributions(group_id);
    tab->SetTitle(title);
    tab->SetURL(url);
    saved_keyed_service_->model()->UpdateTabInGroup(group->saved_guid(), *tab);
  }

  OnTabNavigatedLocally(group->saved_guid(), tab->saved_tab_guid());
}

void TabGroupServiceWrapper::SetFaviconForTab(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    std::optional<gfx::Image> favicon) {
  if (ShouldUseSyncService()) {
    return;
  }

  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  tab->SetFavicon(favicon);
  saved_keyed_service_->model()->UpdateTabInGroup(group->saved_guid(), *tab);
}

void TabGroupServiceWrapper::RemoveTab(const LocalTabGroupID& group_id,
                                       const LocalTabID& tab_id) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  // Copy the guid incase the group is deleted when the last tab is removed.
  base::Uuid sync_id = group->saved_guid();
  base::Uuid sync_tab_id = tab->saved_tab_guid();

  if (ShouldUseSyncService()) {
    sync_service_->RemoveTab(group_id, tab_id);
  } else {
    saved_keyed_service_->UpdateAttributions(group_id);
    saved_keyed_service_->model()->RemoveTabFromGroupLocally(
        group->saved_guid(), tab->saved_tab_guid());
  }

  OnTabRemovedFromGroupLocally(sync_id, sync_tab_id);
}

void TabGroupServiceWrapper::MoveTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     int new_group_index) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());

  if (ShouldUseSyncService()) {
    sync_service_->MoveTab(group_id, tab_id, new_group_index);
  } else {
    const SavedTabGroupTab* tab = group->GetTab(tab_id);
    saved_keyed_service_->UpdateAttributions(group_id, tab_id);
    saved_keyed_service_->model()->MoveTabInGroupTo(
        group->saved_guid(), tab->saved_tab_guid(), new_group_index);
  }

  OnTabsReorderedLocally(group->saved_guid());
}

void TabGroupServiceWrapper::OnTabSelected(const LocalTabGroupID& group_id,
                                           const LocalTabID& tab_id) {
  NOTIMPLEMENTED();
}

std::vector<SavedTabGroup> TabGroupServiceWrapper::GetAllGroups() {
  if (ShouldUseSyncService()) {
    return sync_service_->GetAllGroups();
  }

  return saved_keyed_service_->model()->saved_tab_groups();
}

std::optional<SavedTabGroup> TabGroupServiceWrapper::GetGroup(
    const base::Uuid& guid) {
  if (ShouldUseSyncService()) {
    return sync_service_->GetGroup(guid);
  }

  const SavedTabGroup* group = saved_keyed_service_->model()->Get(guid);
  return group ? std::optional<SavedTabGroup>(*group) : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupServiceWrapper::GetGroup(
    const LocalTabGroupID& local_id) {
  if (ShouldUseSyncService()) {
    return sync_service_->GetGroup(local_id);
  }

  const SavedTabGroup* group = saved_keyed_service_->model()->Get(local_id);
  return group ? std::optional<SavedTabGroup>(*group) : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupServiceWrapper::GetDeletedGroupIds() {
  NOTIMPLEMENTED();
  return std::vector<LocalTabGroupID>();
}

void TabGroupServiceWrapper::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  if (ShouldUseSyncService()) {
    sync_service_->OpenTabGroup(sync_group_id, std::move(context));
  } else {
    TabGroupActionContextDesktop* desktop_context =
        static_cast<TabGroupActionContextDesktop*>(context.get());
    saved_keyed_service_->OpenSavedTabGroupInBrowser(
        desktop_context->browser, sync_group_id,
        desktop_context->opening_source);
  }
}

void TabGroupServiceWrapper::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  if (ShouldUseSyncService()) {
    sync_service_->UpdateLocalTabGroupMapping(sync_id, local_id);
  } else {
    saved_keyed_service_->model()->OnGroupOpenedInTabStrip(sync_id, local_id);
  }
}

void TabGroupServiceWrapper::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id) {
  if (ShouldUseSyncService()) {
    sync_service_->RemoveLocalTabGroupMapping(local_id);
  } else {
    saved_keyed_service_->model()->OnGroupClosedInTabStrip(local_id);
  }
}

void TabGroupServiceWrapper::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  if (ShouldUseSyncService()) {
    sync_service_->UpdateLocalTabId(local_group_id, sync_tab_id, local_tab_id);
  } else {
    std::optional<SavedTabGroup> group = GetGroup(local_group_id);
    const SavedTabGroupTab tab(*group->GetTab(sync_tab_id));
    saved_keyed_service_->model()->UpdateLocalTabId(group->saved_guid(), tab,
                                                    local_tab_id);
  }
}

bool TabGroupServiceWrapper::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  NOTIMPLEMENTED();
  return false;
}

void TabGroupServiceWrapper::RecordTabGroupEvent(
    const EventDetails& event_details) {
  NOTIMPLEMENTED();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupServiceWrapper::GetSavedTabGroupControllerDelegate() {
  return sync_service_->GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupServiceWrapper::GetSharedTabGroupControllerDelegate() {
  return sync_service_->GetSharedTabGroupControllerDelegate();
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupServiceWrapper::CreateScopedLocalObserverPauser() {
  if (ShouldUseSyncService()) {
    return sync_service_->CreateScopedLocalObserverPauser();
  } else {
    return nullptr;
  }
}

void TabGroupServiceWrapper::AddObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::RemoveObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabAddedToGroupLocally(
    const base::Uuid& group_guid) {
  if (ShouldUseSyncService()) {
    return;
  }

  saved_keyed_service_->OnTabAddedToGroupLocally(group_guid);
}

void TabGroupServiceWrapper::OnTabRemovedFromGroupLocally(
    const base::Uuid& group_guid,
    const base::Uuid& tab_guid) {
  if (ShouldUseSyncService()) {
    return;
  }

  saved_keyed_service_->OnTabRemovedFromGroupLocally(group_guid, tab_guid);
}

void TabGroupServiceWrapper::OnTabNavigatedLocally(const base::Uuid& group_guid,
                                                   const base::Uuid& tab_guid) {
  if (ShouldUseSyncService()) {
    return;
  }

  saved_keyed_service_->OnTabNavigatedLocally(group_guid, tab_guid);
}

void TabGroupServiceWrapper::OnTabsReorderedLocally(
    const base::Uuid& group_guid) {
  if (ShouldUseSyncService()) {
    return;
  }

  saved_keyed_service_->OnTabsReorderedLocally(group_guid);
}

void TabGroupServiceWrapper::OnTabGroupVisualsChanged(
    const base::Uuid& group_guid) {
  if (ShouldUseSyncService()) {
    return;
  }

  saved_keyed_service_->OnTabGroupVisualsChanged(group_guid);
}

bool TabGroupServiceWrapper::ShouldUseSyncService() {
  return sync_service_;
}

}  // namespace tab_groups
