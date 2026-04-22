// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "ui/actions/action_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace send_tab_to_self {

DEFINE_USER_DATA(SendTabToSelfToolbarBubbleController);

// static
SendTabToSelfToolbarBubbleController*
SendTabToSelfToolbarBubbleController::From(BrowserWindowInterface* bwi) {
  return Get(bwi->GetUnownedUserDataHost());
}

SendTabToSelfToolbarBubbleController::SendTabToSelfToolbarBubbleController(
    BrowserWindowInterface* bwi)
    : bwi_(CHECK_DEREF(bwi)),
      scoped_unowned_user_data_(bwi->GetUnownedUserDataHost(), *this) {}

SendTabToSelfToolbarBubbleController::~SendTabToSelfToolbarBubbleController() {
  HideBubble();
}

void SendTabToSelfToolbarBubbleController::ShowBubble(
    const SendTabToSelfEntry& entry,
    views::BubbleAnchor anchor) {
  if (IsBubbleShowing()) {
    bubble()->ReplaceEntry(entry);
    return;
  }
  auto bubble_view =
      std::make_unique<SendTabToSelfToolbarBubbleView>(*bwi_, anchor, entry);
  bubble_tracker_.SetView(bubble_view.get());
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view))->Show();
}

SendTabToSelfBubbleView*
SendTabToSelfToolbarBubbleController::ShowDevicePickerBubble(
    content::WebContents* web_contents) {
  auto anchor = ToolbarButtonProvider::From(base::to_address(bwi_))
                    ->GetBubbleAnchor(kActionSendTabToSelf);
  auto bubble_view =
      std::make_unique<send_tab_to_self::SendTabToSelfDevicePickerBubbleView>(
          std::move(anchor), web_contents);
  auto* bubble_view_ptr = bubble_view.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  bubble_view_ptr->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  return bubble_view_ptr;
}

SendTabToSelfBubbleView* SendTabToSelfToolbarBubbleController::ShowPromoBubble(
    content::WebContents* web_contents,
    bool show_signin_button) {
  auto anchor = ToolbarButtonProvider::From(base::to_address(bwi_))
                    ->GetBubbleAnchor(kActionSendTabToSelf);
  auto bubble_view =
      std::make_unique<send_tab_to_self::SendTabToSelfPromoBubbleView>(
          std::move(anchor), web_contents, show_signin_button);
  auto* bubble_view_ptr = bubble_view.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  bubble_view_ptr->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  return bubble_view_ptr;
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
