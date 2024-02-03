// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS_H_

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"

namespace autofill {

// This class displays a confirmation bubble view after a save card upload or
// virtual card enrollment.
class SaveCardAndVirtualCardEnrollConfirmationBubbleViews
    : public AutofillBubbleBase,
      public LocationBarBubbleDelegateView {
 public:
  SaveCardAndVirtualCardEnrollConfirmationBubbleViews(
      views::View* anchor_view,
      content::WebContents* web_contents,
      base::OnceCallback<void(PaymentsBubbleClosedReason)>
          controller_hide_callback);

  SaveCardAndVirtualCardEnrollConfirmationBubbleViews(
      const SaveCardAndVirtualCardEnrollConfirmationBubbleViews&) = delete;
  SaveCardAndVirtualCardEnrollConfirmationBubbleViews& operator=(
      const SaveCardAndVirtualCardEnrollConfirmationBubbleViews&) = delete;

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void WindowClosing() override;

 private:
  ~SaveCardAndVirtualCardEnrollConfirmationBubbleViews() override;

  base::OnceCallback<void(PaymentsBubbleClosedReason)>
      controller_hide_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS_H_
