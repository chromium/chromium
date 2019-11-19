// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"

MediaToolbarButtonController::MediaToolbarButtonController(
    MediaToolbarButtonControllerDelegate* delegate,
    MediaNotificationService* service)
    : delegate_(delegate), service_(service) {
  DCHECK(delegate_);
  service_->AddObserver(this);
  UpdateToolbarButtonState();
}

MediaToolbarButtonController::~MediaToolbarButtonController() {
  service_->RemoveObserver(this);
}

void MediaToolbarButtonController::OnNotificationListChanged() {
  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::OnMediaDialogOpenedOrClosed() {
  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::UpdateToolbarButtonState() {
  if (service_->HasActiveNotifications()) {
    if (delegate_display_state_ != DisplayState::kShown) {
      delegate_->Enable();
      delegate_->Show();
    }
    delegate_display_state_ = DisplayState::kShown;
    return;
  }

  if (!service_->HasFrozenNotifications()) {
    if (delegate_display_state_ != DisplayState::kHidden)
      delegate_->Hide();
    delegate_display_state_ = DisplayState::kHidden;
    return;
  }

  if (!service_->HasOpenDialog()) {
    if (delegate_display_state_ != DisplayState::kDisabled)
      delegate_->Disable();
    delegate_display_state_ = DisplayState::kDisabled;
  }
}
