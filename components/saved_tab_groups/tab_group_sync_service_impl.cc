// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_service_impl.h"

#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"

namespace tab_groups {

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory model_type_store_factory)
    : model_(std::move(model)),
      bridge_(model_.get(),
              std::move(model_type_store_factory),
              std::move(change_processor)) {
  model_->AddObserver(this);
}

TabGroupSyncServiceImpl::~TabGroupSyncServiceImpl() {
  model_->RemoveObserver(this);
}

void TabGroupSyncServiceImpl::AddObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.AddObserver(observer);
}

void TabGroupSyncServiceImpl::RemoveObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

syncer::ModelTypeSyncBridge* TabGroupSyncServiceImpl::bridge() {
  return &bridge_;
}

void TabGroupSyncServiceImpl::AddOrUpdateGroup(SavedTabGroup group) {}

void TabGroupSyncServiceImpl::RemoveGroup(
    const tab_groups::TabGroupId& local_id) {}

std::vector<SavedTabGroup> TabGroupSyncServiceImpl::GetAllGroups() {
  // TODO(b/326546431): Implement.
  return std::vector<SavedTabGroup>();
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const base::Uuid& guid) {
  // TODO(b/326546431): Implement.
  return std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    tab_groups::TabGroupId& local_id) {
  // TODO(b/326546431): Implement.
  return std::nullopt;
}

void TabGroupSyncServiceImpl::SetLocalTabGroupIdForSyncId(
    const base::Uuid& sync_id,
    tab_groups::TabGroupId& local_id) {
  // TODO(b/326546431): Implement.
}

base::Uuid TabGroupSyncServiceImpl::GetSyncIdForLocalTabGroupId(
    tab_groups::TabGroupId& local_id) {
  // TODO(b/326546431): Implement.
  return base::Uuid();
}

base::Uuid TabGroupSyncServiceImpl::GetLocalIdForSyncId(
    const base::Uuid& sync_id) {
  // TODO(b/326546431): Implement.
  return base::Uuid();
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  CHECK(saved_tab_group);
  // TODO(b/326546431): Implement.
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  CHECK(saved_tab_group);
  // TODO(b/326546431): Implement.
}

}  // namespace tab_groups
