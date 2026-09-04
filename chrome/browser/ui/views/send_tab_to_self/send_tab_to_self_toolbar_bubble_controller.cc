// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

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
      scoped_unowned_user_data_(bwi->GetUnownedUserDataHost(), *this) {
  browser_did_close_subscription_ =
      bwi_->RegisterBrowserDidClose(base::BindRepeating(
          &SendTabToSelfToolbarBubbleController::OnBrowserDidClose,
          base::Unretained(this)));
}

SendTabToSelfToolbarBubbleController::~SendTabToSelfToolbarBubbleController() {
  HideBubble();
}

void SendTabToSelfToolbarBubbleController::ShowBubble(
    const SendTabToSelfEntry& entry,
    views::BubbleAnchor anchor) {
  if (IsBubbleShowing()) {
    pending_entries_.push_back(entry);
    return;
  }
  auto bubble_view =
      std::make_unique<SendTabToSelfToolbarBubbleView>(*bwi_, anchor, entry);
  bubble_tracker_.SetView(bubble_view.get());
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  widget_observation_.Observe(widget);
  widget->Show();
}

void SendTabToSelfToolbarBubbleController::HideBubble() {
  if (!IsBubbleShowing()) {
    return;
  }
  bubble_tracker_.view()->GetWidget()->CloseNow();
}

void SendTabToSelfToolbarBubbleController::OnWidgetDestroyed(
    views::Widget* widget) {
  widget_observation_.Reset();

  if (pending_entries_.empty()) {
    return;
  }

  SendTabToSelfEntry next_entry = std::move(pending_entries_.front());
  pending_entries_.pop_front();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SendTabToSelfToolbarBubbleController::ShowPendingBubble,
                     weak_ptr_factory_.GetWeakPtr(), std::move(next_entry)));
}

void SendTabToSelfToolbarBubbleController::ShowPendingBubble(
    const SendTabToSelfEntry& entry) {
  ToolbarButtonProvider* const toolbar_button_provider =
      ToolbarButtonProvider::From(&bwi_.get());
  if (!toolbar_button_provider) {
    return;
  }

  views::BubbleAnchor anchor =
      toolbar_button_provider->GetBubbleAnchor(kActionSendTabToSelf);
  if (anchor.IsNull()) {
    return;
  }

  ShowBubble(entry, anchor);
}

void SendTabToSelfToolbarBubbleController::OnBrowserDidClose(
    BrowserWindowInterface* browser) {
  if (browser != &bwi_.get()) {
    return;
  }
  pending_entries_.clear();
}

bool SendTabToSelfToolbarBubbleController::IsBubbleShowing() const {
  return !!bubble_tracker_.view();
}

}  // namespace send_tab_to_self
