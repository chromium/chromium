// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/session_service_tab_group_sync_observer.h"

#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
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
  TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
  CHECK(tab_group_service);
  tab_group_service->AddObserver(this);
}

SessionServiceTabGroupSyncObserver::~SessionServiceTabGroupSyncObserver() {
  TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
  CHECK(tab_group_service);
  tab_group_service->RemoveObserver(this);
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

  CHECK(tab_strip_model_->SupportsTabGroups());
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
