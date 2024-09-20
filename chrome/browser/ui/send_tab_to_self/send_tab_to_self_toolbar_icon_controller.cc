// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

namespace send_tab_to_self {

SendTabToSelfToolbarIconController::SendTabToSelfToolbarIconController(
    Profile* profile)
    : profile_(profile) {}

void SendTabToSelfToolbarIconController::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  // TODO(crbug.com/40180897): Any entries that were never shown are lost.
  // This is consistent with current behavior and we don't have UI for
  // showing multiple entries with this iteration.
  if (new_entries.empty()) {
    return;
  }

  // Select semi-randomly the first new entry from the list because there is no
  // UI to show multiple entries.
  const SendTabToSelfEntry* new_entry = new_entries.front();

  // If the active browser matches `profile_`, show the toolbar icon.
  // Otherwise, we will store this entry and wait to show on the next active
  // appropriate browser.
  if (features::IsToolbarPinningEnabled()) {
    auto* browser = chrome::FindLastActiveWithProfile(profile_);
    if (browser && browser->IsActive()) {
      ShowToolbarButton(*new_entry, browser);
      return;
    }
  } else {
    if (GetActiveDelegate()) {
      ShowToolbarButton(*new_entry);
      return;
    }
  }

  StorePendingEntry(new_entry);
}

void SendTabToSelfToolbarIconController::StorePendingEntry(
    const SendTabToSelfEntry* new_entry_pending_notification) {
  const bool had_entry_pending_notification = pending_entry_ != nullptr;

  // |pending_entry_| might already be set, but it's better to overwrite
  // it with a fresher value.
  pending_entry_ = std::make_unique<SendTabToSelfEntry>(
      new_entry_pending_notification->GetGUID(),
      new_entry_pending_notification->GetURL(),
      new_entry_pending_notification->GetTitle(),
      new_entry_pending_notification->GetSharedTime(),
      new_entry_pending_notification->GetDeviceName(),
      new_entry_pending_notification->GetTargetDeviceSyncCacheGuid());

  // Prevent adding the observer several times. This might happen when the
  // window is inactive and this method is called more than once (i.e. the
  // server sends multiple entry batches).
  if (!had_entry_pending_notification) {
    BrowserList::AddObserver(this);
  }
}

void SendTabToSelfToolbarIconController::DismissEntries(
    const std::vector<std::string>& guids) {
  auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile_)
                    ->GetSendTabToSelfModel();
  for (const std::string& guid : guids) {
    model->DismissEntry(guid);
  }
}

void SendTabToSelfToolbarIconController::OnBrowserSetLastActive(
    Browser* browser) {
  if (features::IsToolbarPinningEnabled()) {
    if (!GetActiveDelegate() || browser->profile() != profile_.get()) {
      return;
    }
  } else {
    if (!GetActiveDelegate()) {
      return;
    }
  }
  BrowserList::RemoveObserver(this);

  // Reset |pending_entry_| because it's used to determine if the
  // BrowserListObserver is added in `DisplayNewEntries()`.
  std::unique_ptr<SendTabToSelfEntry> entry = std::move(pending_entry_);

  if (!profile_ || !entry) {
    return;
  }

  if (features::IsToolbarPinningEnabled()) {
    ShowToolbarButton(*entry, browser);
    pending_entry_ = nullptr;
  } else {
    if (browser == chrome::FindBrowserWithProfile(profile_)) {
      ShowToolbarButton(*entry);
      pending_entry_ = nullptr;
    }
  }
}

void SendTabToSelfToolbarIconController::ShowToolbarButton(
    const SendTabToSelfEntry& entry,
    Browser* browser) {
  if (features::IsToolbarPinningEnabled()) {
    CHECK(browser);
    auto* container = BrowserView::GetBrowserViewForBrowser(browser)
                          ->toolbar()
                          ->pinned_toolbar_actions_container();
    container->ShowActionEphemerallyInToolbar(kActionSendTabToSelf, true);
    auto* button = container->GetButtonFor(kActionSendTabToSelf);
    CHECK(button);
    browser->browser_window_features()
        ->send_tab_to_self_toolbar_bubble_controller()
        ->ShowBubble(entry, button);
  } else {
    auto* active_delegate = GetActiveDelegate();
    if (!active_delegate) {
      return;
    }
    active_delegate->Show(entry);
  }

  send_tab_to_self::RecordNotificationShown();
}

void SendTabToSelfToolbarIconController::AddDelegate(
    SendTabToSelfToolbarIconControllerDelegate* delegate) {
  delegate_list_.push_back(delegate);
}

void SendTabToSelfToolbarIconController::RemoveDelegate(
    SendTabToSelfToolbarIconControllerDelegate* delegate) {
  for (unsigned int i = 0; i < delegate_list_.size(); i++) {
    if (delegate_list_[i] == delegate) {
      delegate_list_.erase(delegate_list_.begin() + i);
      return;
    }
  }
}

SendTabToSelfToolbarIconControllerDelegate*
SendTabToSelfToolbarIconController::GetActiveDelegate() {
  for (send_tab_to_self::SendTabToSelfToolbarIconControllerDelegate* delegate :
       delegate_list_) {
    if (delegate->IsActive()) {
      return delegate;
    }
  }
  return nullptr;
}

const Profile* SendTabToSelfToolbarIconController::profile() const {
  return profile_;
}

void SendTabToSelfToolbarIconController::LogNotificationOpened() {
  send_tab_to_self::RecordNotificationOpened();
}

void SendTabToSelfToolbarIconController::LogNotificationDismissed() {
  send_tab_to_self::RecordNotificationDismissed();
}

SendTabToSelfToolbarIconController::~SendTabToSelfToolbarIconController() =
    default;

}  // namespace send_tab_to_self
