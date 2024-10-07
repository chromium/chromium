// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/session_service_tab_group_sync_observer.h"

#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/core/session_id.h"

namespace tab_groups {

SessionServiceTabGroupSyncObserver::SessionServiceTabGroupSyncObserver(
    Profile* profile,
    TabStripModel* tab_strip_model,
    SessionID session_id)
    : profile_(profile),
      tab_strip_model_(tab_strip_model),
      session_id_(session_id) {
  // TODO(crbug.com/361110303): Consider consolidating logic by forwarding
  // observer in proxy.
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
    CHECK(tab_group_service);
    tab_group_service->AddObserver(this);
  } else {
    SavedTabGroupKeyedService* saved_tab_group_keyed_service =
        tab_groups::SavedTabGroupServiceFactory::GetForProfile(profile_);
    if (saved_tab_group_keyed_service) {
      saved_tab_group_observation_.Observe(
          saved_tab_group_keyed_service->model());
    }
  }
}

SessionServiceTabGroupSyncObserver::~SessionServiceTabGroupSyncObserver() {
  // TODO(crbug.com/361110303): Consider consolidating logic by forwarding
  // observer in proxy.
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
    CHECK(tab_group_service);
    tab_group_service->RemoveObserver(this);
  } else {
    saved_tab_group_observation_.Reset();
  }
}

void SessionServiceTabGroupSyncObserver::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
  CHECK(tab_group_service);

  const std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(guid);
  CHECK(saved_group);

  UpdateTabGroupSessionMetadata(saved_group->local_group_id(),
                                guid.AsLowercaseString());
}

void SessionServiceTabGroupSyncObserver::SavedTabGroupRemovedLocally(
    const tab_groups::SavedTabGroup& removed_group) {
  UpdateTabGroupSessionMetadata(removed_group.local_group_id(), std::nullopt);
}

void SessionServiceTabGroupSyncObserver::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if (tab_groups::TriggerSource::REMOTE == source) {
    // Do nothing if this change came from sync.
    return;
  }

  UpdateTabGroupSessionMetadata(group.local_group_id(),
                                group.saved_guid().AsLowercaseString());
}

void SessionServiceTabGroupSyncObserver::OnTabGroupRemoved(
    const tab_groups::LocalTabGroupID& local_id,
    tab_groups::TriggerSource source) {
  if (tab_groups::TriggerSource::REMOTE == source) {
    // Do nothing if this change came from sync.
    return;
  }

  UpdateTabGroupSessionMetadata(local_id, std::nullopt);
}

void SessionServiceTabGroupSyncObserver::OnTabGroupLocalIdChanged(
    const base::Uuid& sync_id,
    const std::optional<LocalTabGroupID>& local_id) {
  UpdateTabGroupSessionMetadata(local_id, sync_id.AsLowercaseString());
}

void SessionServiceTabGroupSyncObserver::UpdateTabGroupSessionMetadata(
    const std::optional<LocalTabGroupID> local_id,
    std::optional<std::string> sync_id) {
  if (!local_id.has_value()) {
    return;
  }

  CHECK(tab_strip_model_->group_model());
  if (!tab_strip_model_->group_model()->ContainsTabGroup(local_id.value())) {
    return;
  }

  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (!session_service) {
    return;
  }

  const tab_groups::TabGroupVisualData* visual_data =
      tab_strip_model_->group_model()
          ->GetTabGroup(local_id.value())
          ->visual_data();

  session_service->SetTabGroupMetadata(session_id_, local_id.value(),
                                       visual_data, std::move(sync_id));
}

}  // namespace tab_groups
