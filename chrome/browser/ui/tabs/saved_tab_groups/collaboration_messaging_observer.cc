// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"

#include <set>

#include "base/uuid.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "components/collaboration/public/collaboration_flow_entry_point.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tabs/public/tab_group.h"

using collaboration::messaging::MessagingBackendServiceFactory;
using collaboration::messaging::PersistentNotificationType;

namespace tab_groups {
namespace {

// Returns the local tab group ID from the PersistentMessage.
std::optional<LocalTabGroupID> UnwrapTabGroupID(PersistentMessage message) {
  auto tab_group_metadata = message.attribution.tab_group_metadata;
  if (tab_group_metadata.has_value()) {
    return tab_group_metadata->local_tab_group_id;
  }
  return std::nullopt;
}

// Returns the tab strip index of the tab. If the sync service is not
// available, returns std::nullopt. Asserts that the tab can be found.
std::optional<int> GetTabStripIndex(LocalTabID local_tab_id,
                                    LocalTabGroupID local_tab_group_id) {
  const Browser* const browser_with_local_group_id =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_tab_group_id);
  if (!browser_with_local_group_id) {
    return std::nullopt;
  }

  TabStripModel* tab_strip_model =
      browser_with_local_group_id->tab_strip_model();
  if (!tab_strip_model || !tab_strip_model->SupportsTabGroups()) {
    return std::nullopt;
  }

  const gfx::Range tab_indices = tab_strip_model->group_model()
                                     ->GetTabGroup(local_tab_group_id)
                                     ->ListTabs();
  for (size_t grouped_tab_index = tab_indices.start();
       grouped_tab_index < tab_indices.end(); grouped_tab_index++) {
    const tabs::TabInterface* const tab =
        tab_strip_model->GetTabAtIndex(grouped_tab_index);
    if (tab->GetHandle().raw_value() == local_tab_id) {
      return grouped_tab_index;
    }
  }

  return std::nullopt;
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
std::optional<TabInfo> UnwrapTabInfo(PersistentMessage message) {
  auto local_tab_group_id = UnwrapTabGroupID(message);
  if (!local_tab_group_id.has_value()) {
    return std::nullopt;
  }

  auto tab_metadata = message.attribution.tab_metadata;
  if (!tab_metadata.has_value()) {
    return std::nullopt;
  }

  auto local_tab_id = tab_metadata->local_tab_id;
  if (!local_tab_id.has_value()) {
    return std::nullopt;
  }

  auto tabstrip_index =
      GetTabStripIndex(local_tab_id.value(), local_tab_group_id.value());
  if (!tabstrip_index.has_value()) {
    return std::nullopt;
  }

  return TabInfo(local_tab_id.value(), local_tab_group_id.value(),
                 tabstrip_index.value());
}

}  // namespace

void CollaborationMessagingObserver::HandleDirtyTabGroup(
    PersistentMessage message,
    MessageDisplayStatus display) {
  // TODO(crbug.com/392604409): Refactor to use a TabGroupFeature.
  std::optional<LocalTabGroupID> local_tab_group_id = UnwrapTabGroupID(message);
  if (!local_tab_group_id) {
    return;
  }

  if (Browser* browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(
          local_tab_group_id.value())) {
    browser->tab_strip_model()->SetTabGroupNeedsAttention(
        local_tab_group_id.value(), display == MessageDisplayStatus::kDisplay);
  }
}

void CollaborationMessagingObserver::HandleDirtyTab(
    PersistentMessage message,
    MessageDisplayStatus display) {
  // TODO(crbug.com/392604409): Refactor to use a TabFeature.
  std::optional<TabInfo> tab_info = UnwrapTabInfo(message);
  if (!tab_info) {
    return;
  }

  if (Browser* browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(
          tab_info->local_tab_group_id)) {
    browser->tab_strip_model()->SetTabNeedsAttentionAt(
        tab_info->tabstrip_index, display == MessageDisplayStatus::kDisplay);
  }
}

void CollaborationMessagingObserver::HandleChip(PersistentMessage message,
                                                MessageDisplayStatus display) {
  std::optional<TabInfo> tab_info = UnwrapTabInfo(message);
  if (!tab_info.has_value()) {
    return;
  }

  if (tabs::TabInterface* tab = SavedTabGroupUtils::GetGroupedTab(
          tab_info->local_tab_group_id, tab_info->local_tab_id)) {
    if (display == MessageDisplayStatus::kDisplay) {
      tab->GetTabFeatures()->collaboration_messaging_tab_data()->SetMessage(
          message);
    } else {
      tab->GetTabFeatures()->collaboration_messaging_tab_data()->ClearMessage(
          message);
    }
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
    case PersistentNotificationType::TOMBSTONED:
    case PersistentNotificationType::INSTANT_MESSAGE:
    case PersistentNotificationType::UNDEFINED:
      // These notifications have no associated UI on Desktop.
      // Ignore gracefully.
      return;
  }
}

CollaborationMessagingObserver::CollaborationMessagingObserver(Profile* profile)
    : profile_(profile), instant_message_queue_processor_(profile) {
  // This observer is disabled when Shared Tab Groups is not supported.
  if (!tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    return;
  }

  service_ = MessagingBackendServiceFactory::GetForProfile(profile_);
  CHECK(service_);

  persistent_message_service_observation_.Observe(service_);
  service_->SetInstantMessageDelegate(this);
}

CollaborationMessagingObserver::~CollaborationMessagingObserver() {
  if (service_) {
    service_->SetInstantMessageDelegate(nullptr);
  }
}

void CollaborationMessagingObserver::OnMessagingBackendServiceInitialized() {
  CHECK(service_);
  auto messages = service_->GetMessages(PersistentNotificationType::UNDEFINED);
  for (const auto& message : messages) {
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

void CollaborationMessagingObserver::DisplayInstantaneousMessage(
    InstantMessage message,
    InstantMessageSuccessCallback success_callback) {
  instant_message_queue_processor_.Enqueue(message,
                                           std::move(success_callback));
}

void CollaborationMessagingObserver::HideInstantaneousMessage(
    const std::set<base::Uuid>& message_ids) {
  // TODO(crbug.com/416265338): Implement this.
}

void CollaborationMessagingObserver::ReopenTabForCurrentInstantMessage() {
  CHECK(instant_message_queue_processor_.IsMessageShowing());

  const InstantMessage& message =
      instant_message_queue_processor_.GetCurrentMessage();
  const auto& attribution = message.attributions[0];
  auto tab_metadata = attribution.tab_metadata;
  auto tab_group_metadata = attribution.tab_group_metadata;
  if (!tab_metadata || !tab_group_metadata) {
    return;
  }

  std::optional<LocalTabGroupID> group_id =
      tab_group_metadata->local_tab_group_id;
  std::optional<std::string> tab_url = tab_metadata->last_known_url;
  if (!group_id.has_value() || !tab_url.has_value()) {
    return;
  }

  if (Browser* browser =
          tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
              group_id.value())) {
    SavedTabGroupUtils::OpenTabInBrowser(
        GURL(tab_url.value()), browser, profile_,
        WindowOpenDisposition::NEW_BACKGROUND_TAB, std::nullopt,
        group_id.value());
  }
}

void CollaborationMessagingObserver::ManageSharingForCurrentInstantMessage(
    BrowserWindowInterface* current_browser_window_interface) {
  CHECK(instant_message_queue_processor_.IsMessageShowing());

  const InstantMessage& message =
      instant_message_queue_processor_.GetCurrentMessage();
  auto tab_group_metadata = message.attributions[0].tab_group_metadata;
  if (!tab_group_metadata) {
    return;
  }

  std::optional<LocalTabGroupID> group_id =
      tab_group_metadata->local_tab_group_id;
  if (!group_id.has_value()) {
    // No local group id means the group is not open.
    // Try to open the group first.
    std::optional<base::Uuid> sync_tab_group_id =
        tab_group_metadata->sync_tab_group_id;
    if (!sync_tab_group_id.has_value()) {
      return;
    }
    auto* tab_group_service =
        TabGroupSyncServiceFactory::GetForProfile(profile_);
    CHECK(tab_group_service);
    std::optional<LocalTabGroupID> opened_group_id =
        tab_group_service->OpenTabGroup(
            sync_tab_group_id.value(),
            std::make_unique<TabGroupActionContextDesktop>(
                current_browser_window_interface->GetBrowserForMigrationOnly(),
                OpeningSource::kOpenedFromToastAction));
    if (!opened_group_id) {
      return;
    }

    group_id = opened_group_id;
  }

  if (Browser* browser =
          tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
              group_id.value())) {
    saved_tab_groups::metrics::RecordSharedTabGroupManageType(
        saved_tab_groups::metrics::SharedTabGroupManageTypeDesktop::
            kManageGroupFromUserJoinNotification);

    data_sharing::RequestInfo request_info(group_id.value(),
                                           data_sharing::FlowType::kManage);
    collaboration::CollaborationService* service =
        collaboration::CollaborationServiceFactory::GetForProfile(
            browser->profile());
    std::unique_ptr<CollaborationControllerDelegateDesktop> delegate =
        std::make_unique<CollaborationControllerDelegateDesktop>(browser);
    service->StartShareOrManageFlow(
        std::move(delegate), group_id.value(),
        collaboration::CollaborationServiceShareOrManageEntryPoint::
            kDesktopNotification);
  }
}

}  // namespace tab_groups
