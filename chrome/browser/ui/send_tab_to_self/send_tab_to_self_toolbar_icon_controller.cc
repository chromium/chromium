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

  if (GetActiveDelegate()) {
    ShowToolbarButton(*new_entries.at(0));
  } else {
    entry_ = std::make_unique<SendTabToSelfEntry>(
        new_entries.at(0)->GetGUID(), new_entries.at(0)->GetURL(),
        new_entries.at(0)->GetTitle(), new_entries.at(0)->GetSharedTime(),
        new_entries.at(0)->GetOriginalNavigationTime(),
        new_entries.at(0)->GetDeviceName(),
        new_entries.at(0)->GetTargetDeviceSyncCacheGuid());
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
  if (!GetActiveDelegate())
    return;
  BrowserList::RemoveObserver(this);

  if (!profile_ || !entry_)
    return;
  if (browser == chrome::FindBrowserWithProfile(profile_)) {
    ShowToolbarButton(*entry_);
    entry_ = nullptr;
  }
}

void SendTabToSelfToolbarIconController::ShowToolbarButton(
    const SendTabToSelfEntry& entry) {
  auto* active_delegate = GetActiveDelegate();
  if (!active_delegate) {
    return;
  }

  send_tab_to_self::RecordNotificationShown();
  active_delegate->Show(entry);
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
  for (auto* delegate : delegate_list_) {
    if (delegate->IsActive()) {
      return delegate;
    }
  }
  return nullptr;
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
