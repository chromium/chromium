// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller_delegate.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"

namespace send_tab_to_self {

SendTabToSelfToolbarButtonController::SendTabToSelfToolbarButtonController(
    Profile* profile)
    : profile_(profile) {}

void SendTabToSelfToolbarButtonController::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  ShowToolbarButton();
}

void SendTabToSelfToolbarButtonController::DismissEntries(
    const std::vector<std::string>& guids) {
  NOTIMPLEMENTED();
}

void SendTabToSelfToolbarButtonController::ShowToolbarButton() {
  if (!delegate_)
    return;

  if (delegate_display_state_ != DisplayState::kShown) {
    delegate_->Show();
    delegate_display_state_ = DisplayState::kShown;
  }
}

void SendTabToSelfToolbarButtonController::SetDelegate(
    SendTabToSelfToolbarButtonControllerDelegate* delegate) {
  delegate_ = delegate;
}

void SendTabToSelfToolbarButtonController::UpdateToolbarButtonState() {
  NOTIMPLEMENTED();
}

SendTabToSelfToolbarButtonController::~SendTabToSelfToolbarButtonController() =
    default;

}  // namespace send_tab_to_self
