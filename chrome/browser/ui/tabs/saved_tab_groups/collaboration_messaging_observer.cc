// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"

#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
namespace {

// Get the TabModelStrip that contains this group ID.
TabStripModel* GetTabStripModelWithGroup(LocalTabGroupID local_tab_group_id) {
  const Browser* const browser_with_local_group_id =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_tab_group_id);
  CHECK(browser_with_local_group_id);

  TabStripModel* tab_strip_model =
      browser_with_local_group_id->tab_strip_model();
  CHECK(tab_strip_model && tab_strip_model->SupportsTabGroups());

  return tab_strip_model;
}

// Get the TabStrip that contains this group ID.
TabStrip* GetTabStripWithGroup(LocalTabGroupID local_tab_group_id) {
  const Browser* const browser_with_local_group_id =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_tab_group_id);
  CHECK(browser_with_local_group_id);

  auto* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_with_local_group_id);
  CHECK(browser_view);

  auto* tab_strip = browser_view->tabstrip();
  CHECK(tab_strip);

  return tab_strip;
}

// Returns the local tab group ID from the PersistentMessage.
std::optional<LocalTabGroupID> UnwrapTabGroupID(PersistentMessage message) {
  auto tab_group_metadata = message.attribution.tab_group_metadata;
  CHECK(tab_group_metadata.has_value());
  return tab_group_metadata->local_tab_group_id;
}

// Returns the tab strip index of the tab. If the sync service is not
// available, returns std::nullopt. Asserts that the tab can be found.
std::optional<int> GetTabStripIndex(LocalTabID local_tab_id,
                                    LocalTabGroupID local_tab_group_id,
                                    Profile* profile) {
  auto* tab_group_sync_service =
      SavedTabGroupUtils::GetServiceForProfile(profile);
  if (!tab_group_sync_service) {
    return std::nullopt;
  }

  // Get the index in the tab strip of the first tab in the group.
  auto group_offset_in_tabstrip = GetTabStripWithGroup(local_tab_group_id)
                                      ->GetTabGroup(local_tab_group_id)
                                      ->GetFirstTab();
  CHECK(group_offset_in_tabstrip.has_value());

  // Get the index in the group of the tab.
  auto saved_tab_group = tab_group_sync_service->GetGroup(local_tab_group_id);
  CHECK(saved_tab_group);
  auto tab_offset_in_group = saved_tab_group->GetIndexOfTab(local_tab_id);

  if (!tab_offset_in_group.has_value()) {
    // When hiding a DIRTY_TAB message on a deleted tab (e.g. the tab was
    // deleted by another user), this tab won't be found in the group.
    return std::nullopt;
  }

  auto tabstrip_index =
      group_offset_in_tabstrip.value() + tab_offset_in_group.value();

  return tabstrip_index;
}

// Unwrapped tab information used to access the tab.
struct TabInfo {
  LocalTabID local_tab_id;
  LocalTabGroupID local_tab_group_id;
  int tabstrip_index;
};

// Returns the unwrapped tab information from the PersistentMessage.
// Asserts
// * tab metadata exists and contains the tab ID
// * tab group metadata exists and contains the group ID
// * the tab can be found in the tab strip
std::optional<TabInfo> UnwrapTabInfo(PersistentMessage message,
                                     Profile* profile) {
  // Get tab group ID.
  auto local_tab_group_id = UnwrapTabGroupID(message);
  if (!local_tab_group_id) {
    // The LocalTabGroupID is not guaranteed to be available from the
    // MessagingBackenddService, so we need to handle this case gracefully.
    return std::nullopt;
  }

  // Get tab ID.
  auto tab_metadata = message.attribution.tab_metadata;
  CHECK(tab_metadata.has_value());
  auto local_tab_id = tab_metadata->local_tab_id;
  if (!local_tab_id.has_value()) {
    // The LocalTabID is not guaranteed to be available from the
    // MessagingBackenddService, so we need to handle this case gracefully.
    return std::nullopt;
  }

  // Get tab index.
  auto tabstrip_index =
      GetTabStripIndex(*local_tab_id, *local_tab_group_id, profile);
  if (!tabstrip_index.has_value()) {
    return std::nullopt;
  }

  return TabInfo(*local_tab_id, *local_tab_group_id, *tabstrip_index);
}

}  // namespace

void CollaborationMessagingObserver::HandleDirtyTabGroup(
    PersistentMessage message,
    MessageDisplayStatus display) {
  // DIRTY_TAB_GROUP notifications may not have tab metadata. Only the
  // group metadata is needed here.
  std::optional<LocalTabGroupID> local_tab_group_id = UnwrapTabGroupID(message);
  if (!local_tab_group_id) {
    // The LocalTabGroupID is not guaranteed to be available from the
    // MessagingBackenddService, so we need to handle this case gracefully.
    return;
  }

  GetTabStripWithGroup(*local_tab_group_id)
      ->SetTabGroupNeedsAttention(*local_tab_group_id,
                                  display == MessageDisplayStatus::kDisplay);
}

void CollaborationMessagingObserver::HandleDirtyTab(
    PersistentMessage message,
    MessageDisplayStatus display) {
  std::optional<TabInfo> tab_info = UnwrapTabInfo(message, profile_);
  if (!tab_info.has_value()) {
    return;
  }

  GetTabStripWithGroup(tab_info->local_tab_group_id)
      ->SetTabNeedsAttention(tab_info->tabstrip_index,
                             display == MessageDisplayStatus::kDisplay);
}

void CollaborationMessagingObserver::HandleChip(PersistentMessage message,
                                                MessageDisplayStatus display) {
  std::optional<TabInfo> tab_info = UnwrapTabInfo(message, profile_);
  if (!tab_info.has_value()) {
    return;
  }

  tabs::TabInterface* tab =
      GetTabStripModelWithGroup(tab_info->local_tab_group_id)
          ->GetTabAtIndex(tab_info->tabstrip_index);
  CHECK(tab);
  auto* tab_features = tab->GetTabFeatures();
  CHECK(tab_features);

  if (display == MessageDisplayStatus::kDisplay) {
    tab_features->collaboration_messaging_tab_data()->SetMessage(message);
  } else {
    tab_features->collaboration_messaging_tab_data()->ClearMessage(message);
  }
}

void CollaborationMessagingObserver::DispatchMessage(
    PersistentMessage message,
    MessageDisplayStatus display) {
  using collaboration::messaging::PersistentNotificationType;
  switch (message.type) {
    case PersistentNotificationType::DIRTY_TAB_GROUP:
      HandleDirtyTabGroup(message, display);
      break;
    case PersistentNotificationType::DIRTY_TAB:
      HandleDirtyTab(message, display);
      break;
    case PersistentNotificationType::CHIP:
      HandleChip(message, display);
      break;
    default:
      NOTREACHED();
  }
}

CollaborationMessagingObserver::CollaborationMessagingObserver(Profile* profile)
    : profile_(profile) {
  auto* service =
      collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
          profile_);
  CHECK(service);
  messaging_service_observation_.Observe(service);
}

CollaborationMessagingObserver::~CollaborationMessagingObserver() = default;

void CollaborationMessagingObserver::OnMessagingBackendServiceInitialized() {
  auto* service = messaging_service_observation_.GetSource();
  CHECK(service);
  auto messages = service->GetMessages(std::nullopt);
  for (auto message : messages) {
    DispatchMessage(message, MessageDisplayStatus::kDisplay);
  }
}

void CollaborationMessagingObserver::DisplayPersistentMessage(
    PersistentMessage message) {
  DispatchMessage(message, MessageDisplayStatus::kDisplay);
}

void CollaborationMessagingObserver::HidePersistentMessage(
    PersistentMessage message) {
  DispatchMessage(message, MessageDisplayStatus::kHide);
}

void CollaborationMessagingObserver::DispatchMessageForTests(
    PersistentMessage message,
    bool display) {
  CHECK(tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups());
  DispatchMessage(message, display ? MessageDisplayStatus::kDisplay
                                   : MessageDisplayStatus::kHide);
}

}  // namespace tab_groups
