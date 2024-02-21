// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_card_and_virtual_card_enroll_confirmation_ui_params.h"

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
          controller_hide_callback,
      SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params);

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

  // TODO(crbug.com/1499264): FRIEND_TEST is only temporary and should be
  // removed after the UI elements are actually implemented into the view and
  // can be tested.
  FRIEND_TEST_ALL_PREFIXES(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                           ShowSuccessBubbleViewThenHideBubbleView);
  FRIEND_TEST_ALL_PREFIXES(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                           ShowFailureBubbleViewThenHideBubbleView);

  base::OnceCallback<void(PaymentsBubbleClosedReason)>
      controller_hide_callback_;
  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS_H_
