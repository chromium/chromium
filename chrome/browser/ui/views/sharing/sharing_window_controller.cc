// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_window_controller.h"

#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

DEFINE_USER_DATA(SharingWindowController);

// static
SharingWindowController* SharingWindowController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

SharingWindowController::SharingWindowController(
    BrowserWindowInterface* browser)
    : browser_(*browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

SharingWindowController::~SharingWindowController() = default;

SharingDialog* SharingWindowController::ShowSharingDialog(
    content::WebContents* contents,
    SharingDialogData data) {
  views::BubbleAnchor anchor =
      ToolbarButtonProvider::From(&*browser_)->GetBubbleAnchor(std::nullopt);
  auto dialog_view = std::make_unique<SharingDialogView>(
      std::move(anchor), contents, std::move(data));
  auto* dialog_view_ptr = dialog_view.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(dialog_view))->Show();

  return dialog_view_ptr;
}
