// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_churned_users_bubble_view.h"

#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

PaymentsChurnedUsersBubbleView::PaymentsChurnedUsersBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    PaymentsChurnedUsersBubbleController* controller)
    : AutofillLocationBarBubble(anchor, web_contents), controller_(controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

PaymentsChurnedUsersBubbleView::~PaymentsChurnedUsersBubbleView() = default;

void PaymentsChurnedUsersBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void PaymentsChurnedUsersBubbleView::Hide() {
  CloseBubble();
  controller_ = nullptr;
}

void PaymentsChurnedUsersBubbleView::AddedToWidget() {}

std::u16string PaymentsChurnedUsersBubbleView::GetWindowTitle() const {
  return std::u16string();
}

void PaymentsChurnedUsersBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void PaymentsChurnedUsersBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

}  // namespace autofill

BEGIN_METADATA(autofill, PaymentsChurnedUsersBubbleView)
END_METADATA
