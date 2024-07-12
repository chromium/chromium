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
#include "components/saved_tab_groups/features.h"
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
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::RemoveGroup(const LocalTabGroupID& local_id) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::RemoveGroup(const base::Uuid& sync_id) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const TabGroupVisualData* visual_data) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::AddTab(const LocalTabGroupID& group_id,
                                    const LocalTabID& tab_id,
                                    const std::u16string& title,
                                    GURL url,
                                    std::optional<size_t> position) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::UpdateTab(const LocalTabGroupID& group_id,
                                       const LocalTabID& tab_id,
                                       const std::u16string& title,
                                       GURL url,
                                       std::optional<size_t> position) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::RemoveTab(const LocalTabGroupID& group_id,
                                       const LocalTabID& tab_id) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::MoveTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     int new_group_index) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabSelected(const LocalTabGroupID& group_id,
                                           const LocalTabID& tab_id) {
  NOTIMPLEMENTED();
}

std::vector<SavedTabGroup> TabGroupServiceWrapper::GetAllGroups() {
  NOTIMPLEMENTED();
  return std::vector<SavedTabGroup>();
}

std::optional<SavedTabGroup> TabGroupServiceWrapper::GetGroup(
    const base::Uuid& guid) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<SavedTabGroup> TabGroupServiceWrapper::GetGroup(
    const LocalTabGroupID& local_id) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupServiceWrapper::GetDeletedGroupIds() {
  NOTIMPLEMENTED();
  return std::vector<LocalTabGroupID>();
}

void TabGroupServiceWrapper::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  NOTIMPLEMENTED();
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

void TabGroupServiceWrapper::AddObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::RemoveObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabAddedToGroupLocally(
    const base::Uuid& group_guid) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabRemovedFromGroupLocally(
    const base::Uuid& group_guid,
    const base::Uuid& tab_guid) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabNavigatedLocally(const base::Uuid& group_guid,
                                                   const base::Uuid& tab_guid) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabsReorderedLocally(
    const base::Uuid& group_guid) {
  NOTIMPLEMENTED();
}

void TabGroupServiceWrapper::OnTabGroupVisualsChanged(
    const base::Uuid& group_guid) {
  NOTIMPLEMENTED();
}

bool TabGroupServiceWrapper::ShouldUseSyncService() {
  return sync_service_;
}

}  // namespace tab_groups
