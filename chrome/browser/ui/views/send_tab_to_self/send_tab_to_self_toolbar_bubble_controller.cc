// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace send_tab_to_self {
SendTabToSelfToolbarBubbleController::SendTabToSelfToolbarBubbleController(
    BrowserWindowInterface* bwi)
    : bwi_(CHECK_DEREF(bwi)) {}

SendTabToSelfToolbarBubbleController::~SendTabToSelfToolbarBubbleController() {
  HideBubble();
}

void SendTabToSelfToolbarBubbleController::ShowBubble(
    const SendTabToSelfEntry& entry,
    views::View* anchor_view) {
  if (IsBubbleShowing()) {
    bubble()->ReplaceEntry(entry);
    return;
  }
  auto bubble_view = std::make_unique<SendTabToSelfToolbarBubbleView>(
      *bwi_, anchor_view, entry,
      base::BindOnce([](NavigateParams* params) { Navigate(params); }));
  bubble_tracker_.SetView(bubble_view.get());
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view))->Show();
}

void SendTabToSelfToolbarBubbleController::HideBubble() {
  if (!IsBubbleShowing()) {
    return;
  }
  bubble_tracker_.view()->GetWidget()->CloseNow();
}

bool SendTabToSelfToolbarBubbleController::IsBubbleShowing() const {
  return !!bubble_tracker_.view();
}

}  // namespace send_tab_to_self
