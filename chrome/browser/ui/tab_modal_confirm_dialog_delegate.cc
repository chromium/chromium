// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

#include "chrome/browser/chrome_notification_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

using content::NavigationController;
using content::WebContents;

TabModalConfirmDialogDelegate::TabModalConfirmDialogDelegate(
    WebContents* web_contents)
    : close_delegate_(nullptr), closing_(false) {
  NavigationController* controller = &web_contents->GetController();
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<NavigationController>(controller));
}

TabModalConfirmDialogDelegate::~TabModalConfirmDialogDelegate() {
  // If we end up here, the window has been closed, so make sure we don't close
  // it again.
  close_delegate_ = nullptr;
  // Make sure everything is cleaned up.
  Cancel();
}

void TabModalConfirmDialogDelegate::Cancel() {
  if (closing_)
    return;
  // Make sure we won't do anything when another action occurs.
  closing_ = true;
  OnCanceled();
  CloseDialog();
}

void TabModalConfirmDialogDelegate::Accept() {
  if (closing_)
    return;
  // Make sure we won't do anything when another action occurs.
  closing_ = true;
  OnAccepted();
  CloseDialog();
}

void TabModalConfirmDialogDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_LOAD_START, type);

  // Close the dialog if we load a page (because the action might not apply to
  // the same page anymore).
  Close();
}

void TabModalConfirmDialogDelegate::Close() {
  if (closing_)
    return;
  // Make sure we won't do anything when another action occurs.
  closing_ = true;
  OnClosed();
  CloseDialog();
}

void TabModalConfirmDialogDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  if (closing_)
    return;
  OnLinkClicked(disposition);
}

gfx::Image* TabModalConfirmDialogDelegate::GetIcon() {
  return nullptr;
}

int TabModalConfirmDialogDelegate::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

std::u16string TabModalConfirmDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_OK);
}

std::u16string TabModalConfirmDialogDelegate::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

std::u16string TabModalConfirmDialogDelegate::GetLinkText() const {
  return std::u16string();
}

const char* TabModalConfirmDialogDelegate::GetAcceptButtonIcon() {
  return nullptr;
}

const char* TabModalConfirmDialogDelegate::GetCancelButtonIcon() {
  return nullptr;
}

void TabModalConfirmDialogDelegate::OnAccepted() {}

void TabModalConfirmDialogDelegate::OnCanceled() {}

void TabModalConfirmDialogDelegate::OnLinkClicked(
    WindowOpenDisposition disposition) {}

void TabModalConfirmDialogDelegate::OnClosed() {}

void TabModalConfirmDialogDelegate::CloseDialog() {
  if (close_delegate_)
    close_delegate_->CloseDialog();
}

absl::optional<int> TabModalConfirmDialogDelegate::GetDefaultDialogButton() {
  // Use the default, don't override.
  return absl::nullopt;
}

absl::optional<int> TabModalConfirmDialogDelegate::GetInitiallyFocusedButton() {
  // Use the default, don't override.
  return absl::nullopt;
}
