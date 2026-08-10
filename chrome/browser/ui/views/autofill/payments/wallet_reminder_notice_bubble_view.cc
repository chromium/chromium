// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/wallet_reminder_notice_bubble_view.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

WalletReminderNoticeBubbleView::WalletReminderNoticeBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    WalletReminderNoticeBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {}

WalletReminderNoticeBubbleView::~WalletReminderNoticeBubbleView() = default;

void WalletReminderNoticeBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void WalletReminderNoticeBubbleView::Hide() {
  CloseBubble();
  WindowClosing();
}

std::u16string WalletReminderNoticeBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/543473467): Retrieve the window title via the controller.
  return std::u16string();
}

void WalletReminderNoticeBubbleView::WindowClosing() {
  // TODO(crbug.com/543473467): Handle closing the bubble via the controller.
}

void WalletReminderNoticeBubbleView::Init() {
  // TODO(crbug.com/543473467): Display Wallet reminder notice UI.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

BEGIN_METADATA(WalletReminderNoticeBubbleView)
END_METADATA

}  // namespace autofill
