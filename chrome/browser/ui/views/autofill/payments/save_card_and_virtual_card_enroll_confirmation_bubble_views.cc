// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_and_virtual_card_enroll_confirmation_bubble_views.h"

#include <utility>

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "ui/base/ui_base_types.h"

namespace autofill {

SaveCardAndVirtualCardEnrollConfirmationBubbleViews::
    SaveCardAndVirtualCardEnrollConfirmationBubbleViews(
        views::View* anchor_view,
        content::WebContents* web_contents,
        base::OnceCallback<void(PaymentsBubbleClosedReason)>
            controller_hide_callback,
        SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_hide_callback_(std::move(controller_hide_callback)),
      ui_params_(std::move(ui_params)) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void SaveCardAndVirtualCardEnrollConfirmationBubbleViews::Hide() {
  CloseBubble();
  if (!controller_hide_callback_.is_null()) {
    std::move(controller_hide_callback_)
        .Run(GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
}

void SaveCardAndVirtualCardEnrollConfirmationBubbleViews::WindowClosing() {
  if (!controller_hide_callback_.is_null()) {
    std::move(controller_hide_callback_)
        .Run(GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
}

SaveCardAndVirtualCardEnrollConfirmationBubbleViews::
    ~SaveCardAndVirtualCardEnrollConfirmationBubbleViews() = default;

}  // namespace autofill
