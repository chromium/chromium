// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "components/global_media_controls/public/media_item_manager.h"

MediaToolbarButtonController::MediaToolbarButtonController(
    MediaToolbarButtonControllerDelegate* delegate,
    global_media_controls::MediaItemManager* item_manager)
    : delegate_(delegate), item_manager_(item_manager) {
  DCHECK(delegate_);
  item_manager_->AddObserver(this);
  UpdateToolbarButtonState();
}

MediaToolbarButtonController::~MediaToolbarButtonController() {
  item_manager_->RemoveObserver(this);
}

void MediaToolbarButtonController::OnItemListChanged() {
  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::OnMediaDialogOpened() {
  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::OnMediaDialogClosed() {
  UpdateToolbarButtonState();
  // Triggered when media playback is active and a casting session is initiated,
  // prompting the user to manage their casting session.
  delegate_->MaybeShowStopCastingPromo();
  // Triggered exclusively for local media content, encourages the user to begin
  // casting if a compatible sink is available.
  delegate_->MaybeShowLocalMediaCastingPromo();
}

void MediaToolbarButtonController::ShowToolbarButton() {
  if (delegate_display_state_ != DisplayState::kShown) {
    delegate_->Show();
    delegate_->Enable();
    delegate_display_state_ = DisplayState::kShown;
  }
}

void MediaToolbarButtonController::UpdateToolbarButtonState() {
  if (item_manager_->HasActiveItems() || item_manager_->HasOpenDialog()) {
    ShowToolbarButton();
    return;
  }

  if (!item_manager_->HasFrozenItems()) {
    if (delegate_display_state_ != DisplayState::kHidden)
      delegate_->Hide();
    delegate_display_state_ = DisplayState::kHidden;
    return;
  }

  if (!item_manager_->HasOpenDialog()) {
    if (delegate_display_state_ != DisplayState::kDisabled)
      delegate_->Disable();
    delegate_display_state_ = DisplayState::kDisabled;
  }
}
