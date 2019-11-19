// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestErrorMessageTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestErrorMessageTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestErrorMessageTest);
};

// Testing the use of the complete('fail') JS API and the error message.
IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest, CompleteFail) {
  NavigateTo("/payment_request_fail_complete_test.html");
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa
  card.set_billing_address_id(billing_profile.guid());
  AddCreditCard(card);
  InvokePaymentRequestUI();

  // We are ready to pay.
  ASSERT_TRUE(IsPayButtonEnabled());

  // Once "Pay" is clicked, the page will call complete('fail') and the error
  // message should be shown.
  OpenCVCPromptWithCVC(base::ASCIIToUTF16("123"));

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON);
  EXPECT_FALSE(dialog_view()->throbber_overlay_for_testing()->GetVisible());

  // The user can only close the dialog at this point.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

}  // namespace payments
