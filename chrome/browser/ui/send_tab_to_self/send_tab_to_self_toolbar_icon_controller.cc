// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller_delegate.h"
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
  // TODO(crbug/1206381): Any entries that were never shown are lost.
  // This is consistent with current behavior and we don't have UI for
  // showing multiple entries with this iteration.
  if (new_entries.empty())
    return;

  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (platform_util::IsWindowActive(browser->window()->GetNativeWindow())) {
    ShowToolbarButton(*new_entries.at(0));
    return;
  }

  const bool had_entry_pending_notification = entry_ != nullptr;

  // Select semi-randomly the first new entry from the list because there is no
  // UI to show multiple entries.
  const SendTabToSelfEntry* new_entry_pending_notification =
      new_entries.front();

  // |entry_| might already be set, but it's better to overwrite it with a
  // fresher value.
  entry_ = std::make_unique<SendTabToSelfEntry>(
      new_entry_pending_notification->GetGUID(),
      new_entry_pending_notification->GetURL(),
      new_entry_pending_notification->GetTitle(),
      new_entry_pending_notification->GetSharedTime(),
      new_entry_pending_notification->GetOriginalNavigationTime(),
      new_entry_pending_notification->GetDeviceName(),
      new_entry_pending_notification->GetTargetDeviceSyncCacheGuid());

  // Prevent adding the observer several times. This might happen when the
  // window is inactive and this method is called more than once (i.e. the
  // server sends multiple entry batches).
  if (!had_entry_pending_notification)
    BrowserList::AddObserver(this);
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
  BrowserList::RemoveObserver(this);

  // Reset |entry_| because it's used to determine if the BrowserListObserver is
  // added in DisplayNewEntries().
  std::unique_ptr<SendTabToSelfEntry> entry = std::move(entry_);

  if (!profile_ || !entry)
    return;
  if (browser == chrome::FindBrowserWithProfile(profile_))
    ShowToolbarButton(*entry);
}

void SendTabToSelfToolbarIconController::ShowToolbarButton(
    const SendTabToSelfEntry& entry) {
  if (!delegate_)
    return;

  send_tab_to_self::RecordNotificationShown();
  delegate_->Show(entry);
}

void SendTabToSelfToolbarIconController::SetDelegate(
    SendTabToSelfToolbarIconControllerDelegate* delegate) {
  delegate_ = delegate;
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
