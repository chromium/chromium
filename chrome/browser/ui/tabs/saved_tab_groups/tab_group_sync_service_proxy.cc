// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace tab_groups {

TabGroupSyncServiceProxy::TabGroupSyncServiceProxy(
    SavedTabGroupKeyedService* service) {
  CHECK(service);

  service_ = service;
}

TabGroupSyncServiceProxy::~TabGroupSyncServiceProxy() = default;

void TabGroupSyncServiceProxy::SetTabGroupSyncDelegate(
    std::unique_ptr<TabGroupSyncDelegate> delegate) {
  NOTIMPLEMENTED();
}

void TabGroupSyncServiceProxy::AddGroup(SavedTabGroup group) {
  service_->model()->AddedLocally(std::move(group));
}

void TabGroupSyncServiceProxy::RemoveGroup(const LocalTabGroupID& local_id) {
  service_->UnsaveGroup(local_id, ClosingSource::kDeletedByUser);
}

void TabGroupSyncServiceProxy::RemoveGroup(const base::Uuid& sync_id) {
  service_->model()->RemovedLocally(sync_id);
}

void TabGroupSyncServiceProxy::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const TabGroupVisualData* visual_data) {
  service_->UpdateAttributions(local_group_id);
  service_->model()->UpdateVisualDataLocally(local_group_id, visual_data);

  std::optional<SavedTabGroup> group = GetGroup(local_group_id);
  CHECK(group.has_value());
  service_->OnTabGroupVisualsChanged(group->saved_guid());
}

void TabGroupSyncServiceProxy::UpdateGroupPosition(
    const base::Uuid& sync_id,
    std::optional<bool> is_pinned,
    std::optional<int> new_index) {
  std::optional<SavedTabGroup> group = GetGroup(sync_id);
  if (!group.has_value()) {
    return;
  }

  if (is_pinned.has_value() && group->is_pinned() != is_pinned) {
    service_->model()->TogglePinState(sync_id);
  }

  if (new_index.has_value()) {
    service_->model()->ReorderGroupLocally(sync_id, new_index.value());
  }
}

void TabGroupSyncServiceProxy::AddTab(const LocalTabGroupID& group_id,
                                      const LocalTabID& tab_id,
                                      const std::u16string& title,
                                      const GURL& url,
                                      std::optional<size_t> position) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());

  SavedTabGroupTab new_tab(url, title, group->saved_guid(), position,
                           /*saved_tab_guid=*/std::nullopt, tab_id);
  new_tab.SetLocalTabID(tab_id);
  if (position.has_value()) {
    new_tab.SetPosition(position.value());
  }
  new_tab.SetCreatorCacheGuid(service_->GetLocalCacheGuid());
  service_->UpdateAttributions(group_id);
  service_->model()->AddTabToGroupLocally(group->saved_guid(), new_tab);

  service_->OnTabAddedToGroupLocally(group->saved_guid());
}

void TabGroupSyncServiceProxy::NavigateTab(const LocalTabGroupID& group_id,
                                           const LocalTabID& tab_id,
                                           const GURL& url,
                                           const std::u16string& title) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  SavedTabGroupTab updated_tab(*tab);
  updated_tab.SetURL(url);
  updated_tab.SetTitle(title);

  service_->model()->UpdateTabInGroup(group->saved_guid(),
                                      std::move(updated_tab),
                                      /*notify_observers=*/true);

  service_->OnTabNavigatedLocally(group->saved_guid(), tab->saved_tab_guid());
}

void TabGroupSyncServiceProxy::UpdateTabProperties(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    const SavedTabGroupTabBuilder& tab_builder) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  service_->UpdateAttributions(group_id);
  service_->model()->UpdateTabInGroup(group->saved_guid(),
                                      tab_builder.Build(*tab),
                                      /*notify_observers=*/false);

  service_->OnTabNavigatedLocally(group->saved_guid(), tab->saved_tab_guid());
}

void TabGroupSyncServiceProxy::SetFaviconForTab(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    std::optional<gfx::Image> favicon) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  tab->SetFavicon(favicon);
  service_->model()->UpdateTabInGroup(group->saved_guid(), *tab,
                                      /*notify_observers=*/false);
}

void TabGroupSyncServiceProxy::RemoveTab(const LocalTabGroupID& group_id,
                                         const LocalTabID& tab_id) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());
  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  // Copy the guid incase the group is deleted when the last tab is removed.
  base::Uuid sync_id = group->saved_guid();
  base::Uuid sync_tab_id = tab->saved_tab_guid();

  service_->UpdateAttributions(group_id);
  service_->model()->RemoveTabFromGroupLocally(group->saved_guid(),
                                               tab->saved_tab_guid());

  service_->OnTabRemovedFromGroupLocally(sync_id, sync_tab_id);
}

void TabGroupSyncServiceProxy::MoveTab(const LocalTabGroupID& group_id,
                                       const LocalTabID& tab_id,
                                       int new_group_index) {
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  CHECK(group.has_value());

  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  service_->UpdateAttributions(group_id, tab_id);
  service_->model()->MoveTabInGroupTo(group->saved_guid(),
                                      tab->saved_tab_guid(), new_group_index);

  service_->OnTabsReorderedLocally(group->saved_guid());
}

void TabGroupSyncServiceProxy::OnTabSelected(const LocalTabGroupID& group_id,
                                             const LocalTabID& tab_id) {
  NOTIMPLEMENTED();
}

void TabGroupSyncServiceProxy::SaveGroup(SavedTabGroup group) {
  service_->SaveRestoredGroup(std::move(group));
}

void TabGroupSyncServiceProxy::UnsaveGroup(const LocalTabGroupID& local_id) {
  service_->UnsaveGroup(local_id, ClosingSource::kClosedByUser);
}

void TabGroupSyncServiceProxy::MakeTabGroupShared(
    const LocalTabGroupID& local_group_id,
    std::string_view collaboration_id) {
  NOTIMPLEMENTED();
}

std::vector<SavedTabGroup> TabGroupSyncServiceProxy::GetAllGroups() const {
  return service_->model()->saved_tab_groups();
}

std::optional<SavedTabGroup> TabGroupSyncServiceProxy::GetGroup(
    const base::Uuid& guid) const {
  const SavedTabGroup* group = service_->model()->Get(guid);
  return group ? std::optional<SavedTabGroup>(*group) : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceProxy::GetGroup(
    const LocalTabGroupID& local_id) const {
  const SavedTabGroup* group = service_->model()->Get(local_id);
  return group ? std::optional<SavedTabGroup>(*group) : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceProxy::GetDeletedGroupIds()
    const {
  NOTIMPLEMENTED();
  return std::vector<LocalTabGroupID>();
}

void TabGroupSyncServiceProxy::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  TabGroupActionContextDesktop* desktop_context =
      static_cast<TabGroupActionContextDesktop*>(context.get());
  service_->OpenSavedTabGroupInBrowser(desktop_context->browser, sync_group_id,
                                       desktop_context->opening_source);
}

void TabGroupSyncServiceProxy::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  service_->model()->OnGroupOpenedInTabStrip(sync_id, local_id);
}

void TabGroupSyncServiceProxy::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id,
    ClosingSource closing_source) {
  service_->model()->OnGroupClosedInTabStrip(local_id);
}

void TabGroupSyncServiceProxy::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  std::optional<SavedTabGroup> group = GetGroup(local_group_id);
  const SavedTabGroupTab tab(*group->GetTab(sync_tab_id));
  service_->model()->UpdateLocalTabId(group->saved_guid(), tab, local_tab_id);
}

void TabGroupSyncServiceProxy::ConnectLocalTabGroup(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  service_->ConnectRestoredGroupToSaveId(sync_id, local_id);
}

bool TabGroupSyncServiceProxy::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  NOTIMPLEMENTED();
  return false;
}

bool TabGroupSyncServiceProxy::WasTabGroupClosedLocally(
    const base::Uuid& sync_id) const {
  NOTIMPLEMENTED();
  return false;
}

void TabGroupSyncServiceProxy::RecordTabGroupEvent(
    const EventDetails& event_details) {
  NOTIMPLEMENTED();
}

TabGroupSyncMetricsLogger*
TabGroupSyncServiceProxy::GetTabGroupSyncMetricsLogger() {
  return service_->GetTabGroupSyncMetricsLogger();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceProxy::GetSavedTabGroupControllerDelegate() {
  return service_->GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceProxy::GetSharedTabGroupControllerDelegate() {
  return service_->GetSharedTabGroupControllerDelegate();
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncServiceProxy::CreateScopedLocalObserverPauser() {
  return service_->CreateScopedLocalObserverPauser();
}

void TabGroupSyncServiceProxy::GetURLRestriction(
    const GURL& url,
    TabGroupSyncService::UrlRestrictionCallback callback) {
  std::move(callback).Run(std::nullopt);
}

std::unique_ptr<std::vector<SavedTabGroup>>
TabGroupSyncServiceProxy::TakeSharedTabGroupsAvailableAtStartupForMessaging() {
  // This method should only exist and be used in the underlying service.
  NOTREACHED();
}

void TabGroupSyncServiceProxy::AddObserver(Observer* observer) {
  if (observers_.empty()) {
    service_->model()->AddObserver(this);
  }

  observers_.AddObserver(observer);
}

void TabGroupSyncServiceProxy::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

  if (observers_.empty()) {
    service_->model()->RemoveObserver(this);
  }
}

void TabGroupSyncServiceProxy::SetIsInitializedForTesting(bool initialized) {
  service_->model()->LoadStoredEntries({}, {});
}

void TabGroupSyncServiceProxy::SavedTabGroupModelLoaded() {
  for (auto& observer : observers_) {
    observer.OnInitialized();
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  const SavedTabGroup* group = service_->model()->Get(guid);
  CHECK(group);
  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(*group, TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  const SavedTabGroup* group = service_->model()->Get(guid);
  CHECK(group);
  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(*group, TriggerSource::REMOTE);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group.saved_guid(),
                               TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group.saved_guid(),
                               TriggerSource::REMOTE);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupLocalIdChanged(
    const base::Uuid& saved_group_id) {
  const SavedTabGroup* group = service_->model()->Get(saved_group_id);
  CHECK(group);
  for (auto& observer : observers_) {
    observer.OnTabGroupLocalIdChanged(saved_group_id, group->local_group_id());
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* group = service_->model()->Get(group_guid);
  CHECK(group);
  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*group, TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* group = service_->model()->Get(group_guid);
  CHECK(group);
  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*group, TriggerSource::REMOTE);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupReorderedLocally() {
  for (auto& observer : observers_) {
    observer.OnTabGroupsReordered(TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceProxy::SavedTabGroupReorderedFromSync() {
  for (auto& observer : observers_) {
    observer.OnTabGroupsReordered(TriggerSource::REMOTE);
  }
}

}  // namespace tab_groups
