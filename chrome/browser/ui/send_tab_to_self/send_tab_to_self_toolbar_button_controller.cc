// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller_delegate.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

namespace send_tab_to_self {

SendTabToSelfToolbarButtonController::SendTabToSelfToolbarButtonController(
    Profile* profile)
    : profile_(profile) {}

void SendTabToSelfToolbarButtonController::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  // TODO(crbug/1206381): Any entries that were never shown are lost.
  // This is consistent with current behavior and we don't have UI for
  // showing multiple entries with this iteration.
  if (!new_entries.empty()) {
    ShowToolbarButton(*new_entries.at(0));
  }
}

void SendTabToSelfToolbarButtonController::DismissEntries(
    const std::vector<std::string>& guids) {
  auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile_)
                    ->GetSendTabToSelfModel();
  for (const std::string& guid : guids) {
    model->DismissEntry(guid);
  }
}

void SendTabToSelfToolbarButtonController::ShowToolbarButton(
    const SendTabToSelfEntry& entry) {
  if (!delegate_)
    return;

  send_tab_to_self::RecordNotificationShown();
  delegate_->Show(entry);
}

void SendTabToSelfToolbarButtonController::SetDelegate(
    SendTabToSelfToolbarButtonControllerDelegate* delegate) {
  delegate_ = delegate;
}

void SendTabToSelfToolbarButtonController::LogNotificationOpened() {
  send_tab_to_self::RecordNotificationOpened();
}

void SendTabToSelfToolbarButtonController::LogNotificationDismissed() {
  send_tab_to_self::RecordNotificationDismissed();
}

SendTabToSelfToolbarButtonController::~SendTabToSelfToolbarButtonController() =
    default;

}  // namespace send_tab_to_self
