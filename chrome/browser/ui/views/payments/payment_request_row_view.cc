// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_row_view.h"

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace payments {

PaymentRequestRowView::PaymentRequestRowView(views::ButtonListener* listener,
                                             bool clickable,
                                             const gfx::Insets& insets)
    : views::Button(listener),
      clickable_(clickable),
      insets_(insets),
      previous_row_(nullptr) {
  SetEnabled(clickable_);
  ShowBottomSeparator();
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
}

PaymentRequestRowView::~PaymentRequestRowView() {}

void PaymentRequestRowView::SetActiveBackground() {
  // TODO(crbug/976890): Check whether we can GetSystemColor from a NativeTheme
  // ColorId instead of hard code here.
  SetBackground(views::CreateSolidBackground(SkColorSetA(SK_ColorBLACK, 0x0D)));
}

void PaymentRequestRowView::ShowBottomSeparator() {
  SetBorder(payments::CreatePaymentRequestRowBorder(
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_SeparatorColor),
      insets_));
  SchedulePaint();
}

void PaymentRequestRowView::HideBottomSeparator() {
  SetBorder(views::CreateEmptyBorder(insets_));
  SchedulePaint();
}

void PaymentRequestRowView::SetIsHighlighted(bool highlighted) {
  if (highlighted) {
    SetActiveBackground();
    HideBottomSeparator();
    if (previous_row_)
      previous_row_->HideBottomSeparator();
  } else {
    SetBackground(nullptr);
    ShowBottomSeparator();
    if (previous_row_)
      previous_row_->ShowBottomSeparator();
  }
}

// views::Button:
void PaymentRequestRowView::StateChanged(ButtonState old_state) {
  if (!clickable())
    return;

  SetIsHighlighted(state() == views::Button::STATE_HOVERED ||
                   state() == views::Button::STATE_PRESSED);
}

void PaymentRequestRowView::OnFocus() {
  if (clickable()) {
    SetIsHighlighted(true);
    SchedulePaint();
  }
}

void PaymentRequestRowView::OnBlur() {
  if (clickable()) {
    SetIsHighlighted(false);
    SchedulePaint();
  }
}

}  // namespace payments
