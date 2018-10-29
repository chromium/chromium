// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/popups_only_ui_delegate.h"

#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/desktop_popup_alignment_delegate.h"
#include "ui/message_center/views/message_popup_collection.h"

// static
std::unique_ptr<PopupsOnlyUiController::Delegate>
PopupsOnlyUiController::CreateDelegate() {
  return std::make_unique<PopupsOnlyUiDelegate>();
}

PopupsOnlyUiDelegate::PopupsOnlyUiDelegate() {
  alignment_delegate_.reset(new message_center::DesktopPopupAlignmentDelegate);
  popup_collection_.reset(
      new message_center::MessagePopupCollection(alignment_delegate_.get()));
}

PopupsOnlyUiDelegate::~PopupsOnlyUiDelegate() {
  // Reset this early so that delegated events during destruction don't cause
  // problems.
  popup_collection_.reset();
}

void PopupsOnlyUiDelegate::ShowPopups() {
  alignment_delegate_->StartObserving(display::Screen::GetScreen());
}

void PopupsOnlyUiDelegate::HidePopups() {
  DCHECK(popup_collection_.get());
}
