// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/blocked_action_bubble_delegate.h"

#include <string>
#include <utility>

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

BlockedActionBubbleDelegate::BlockedActionBubbleDelegate(
    base::OnceCallback<void(CloseAction)> callback,
    const std::string& extension_id)
    : callback_(std::move(callback)), extension_id_(extension_id) {}

BlockedActionBubbleDelegate::~BlockedActionBubbleDelegate() {}

bool BlockedActionBubbleDelegate::ShouldShow() {
  // TODO(devlin): Technically, this could be wrong if the extension no longer
  // wants to run, was unloaded, etc. We should update this.
  return true;
}

bool BlockedActionBubbleDelegate::ShouldCloseOnDeactivate() {
  return true;
}

std::u16string BlockedActionBubbleDelegate::GetHeadingText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_HEADING);
}

std::u16string BlockedActionBubbleDelegate::GetBodyText(
    bool anchored_to_action) {
  return std::u16string();
}

std::u16string BlockedActionBubbleDelegate::GetItemListText() {
  return std::u16string();  // No item list.
}

std::u16string BlockedActionBubbleDelegate::GetActionButtonText() {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON);
}

std::u16string BlockedActionBubbleDelegate::GetDismissButtonText() {
  return std::u16string();
}

ui::DialogButton BlockedActionBubbleDelegate::GetDefaultDialogButton() {
  return ui::DIALOG_BUTTON_OK;
}

std::string BlockedActionBubbleDelegate::GetAnchorActionId() {
  return extension_id_;
}

void BlockedActionBubbleDelegate::OnBubbleShown(
    base::OnceClosure close_bubble_callback) {}

void BlockedActionBubbleDelegate::OnBubbleClosed(CloseAction action) {
  std::move(callback_).Run(action);
}

std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
BlockedActionBubbleDelegate::GetExtraViewInfo() {
  return nullptr;
}
