// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"

namespace autofill {

OfferNotificationBubbleViews::OfferNotificationBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetOkButtonLabel());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
}

OfferNotificationBubbleViews::~OfferNotificationBubbleViews() {
  Hide();
}

void OfferNotificationBubbleViews::Hide() {
  CloseBubble();
  if (controller_)
    controller_->OnBubbleClosed(closed_reason_);
  controller_ = nullptr;
}

void OfferNotificationBubbleViews::Init() {
  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  // TODO(crbug.com/1093057): Implement the explanation text view.
}

void OfferNotificationBubbleViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(GetWindowTitle()));
}

base::string16 OfferNotificationBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

void OfferNotificationBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(closed_reason_);
    controller_ = nullptr;
  }
}

void OfferNotificationBubbleViews::OnWidgetClosing(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetClosing(widget);
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kCancelButtonClicked);
  closed_reason_ = GetPaymentsBubbleClosedReasonFromWidgetClosedReason(
      widget->closed_reason());
}

}  // namespace autofill
