// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_row_view.h"

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace payments {

PaymentRequestRowView::PaymentRequestRowView(PressedCallback callback,
                                             bool clickable,
                                             const gfx::Insets& insets)
    : views::Button(std::move(callback)),
      clickable_(clickable),
      insets_(insets),
      previous_row_(nullptr) {
  // When not clickable, use Button's STATE_DISABLED but don't set our
  // View state to disabled. The former ensures we aren't clickable, the
  // latter also disables us and our children for event handling.
  views::Button::SetState(clickable_ ? views::Button::STATE_NORMAL
                                     : views::Button::STATE_DISABLED);
  ShowBottomSeparator();
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
}

PaymentRequestRowView::~PaymentRequestRowView() {}

bool PaymentRequestRowView::GetClickable() const {
  return clickable_;
}

void PaymentRequestRowView::SetActiveBackground() {
  // TODO(crbug/976890): Check whether we can GetSystemColor from a NativeTheme
  // ColorId instead of hard code here.
  SetBackground(views::CreateSolidBackground(SkColorSetA(SK_ColorBLACK, 0x0D)));
}

void PaymentRequestRowView::ShowBottomSeparator() {
  bottom_separator_visible_ = true;
  UpdateBottomSeparator();
  SchedulePaint();
}

void PaymentRequestRowView::HideBottomSeparator() {
  bottom_separator_visible_ = false;
  UpdateBottomSeparator();
  SchedulePaint();
}

void PaymentRequestRowView::UpdateBottomSeparator() {
  // Create an empty border even when not present in a Widget hierarchy as the
  // border is needed to correctly compute the bounds of the ScrollView in the
  // PaymentRequestSheetController which is done before this is added to its
  // Widget.
  // TODO(crbug.com/1213247): Update PaymentRequestSheetController to recompute
  // the bounds of its ScrollView in response to changes in preferred size.
  SetBorder(bottom_separator_visible_ && GetWidget()
                ? payments::CreatePaymentRequestRowBorder(
                      GetNativeTheme()->GetSystemColor(
                          ui::NativeTheme::kColorId_SeparatorColor),
                      insets_)
                : views::CreateEmptyBorder(insets_));
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

void PaymentRequestRowView::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  if (!GetClickable())
    return;

  SetIsHighlighted(GetState() == views::Button::STATE_HOVERED ||
                   GetState() == views::Button::STATE_PRESSED);
}

void PaymentRequestRowView::OnThemeChanged() {
  Button::OnThemeChanged();
  UpdateBottomSeparator();
}

void PaymentRequestRowView::OnFocus() {
  if (GetClickable()) {
    SetIsHighlighted(true);
    SchedulePaint();
  }
}

void PaymentRequestRowView::OnBlur() {
  if (GetClickable()) {
    SetIsHighlighted(false);
    SchedulePaint();
  }
}

BEGIN_METADATA(PaymentRequestRowView, views::Button)
ADD_READONLY_PROPERTY_METADATA(bool, Clickable)
END_METADATA

}  // namespace payments
