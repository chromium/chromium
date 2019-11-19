// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class PaymentRequestCvcUnmaskViewControllerTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCvcUnmaskViewControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCvcUnmaskViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       CvcSentToResponse) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("012"));

  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

// Test is flaky crbug.com/814313
#if defined(OS_WIN)
#define MAYBE_OpenGoBackOpenPay DISABLED_OpenGoBackOpenPay
#else
#define MAYBE_OpenGoBackOpenPay OpenGoBackOpenPay
#endif  // defined(OS_WIN)
// Test that going in the CVC editor, backing out and opening it again to pay
// does not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       MAYBE_OpenGoBackOpenPay) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  OpenCVCPromptWithCVC(base::ASCIIToUTF16("012"));

  // Go back before confirming the CVC.
  ClickOnBackArrow();

  // Now pay for real.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("012"));
  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       EnterAcceleratorConfirmsCvc) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  // This prevents a timeout in error cases where PAY_BUTTON is disabled.
  ASSERT_TRUE(dialog_view()
                  ->GetViewByID(static_cast<int>(DialogViewID::PAY_BUTTON))
                  ->GetEnabled());
  OpenCVCPromptWithCVC(base::ASCIIToUTF16("012"));

  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  views::View* cvc_sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CVC_UNMASK_SHEET));
  cvc_sheet->AcceleratorPressed(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  WaitForAnimation();
  WaitForObservedEvent();

  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       ButtonDisabled) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  // This prevents a timeout in error cases where PAY_BUTTON is disabled.
  ASSERT_TRUE(dialog_view()
                  ->GetViewByID(static_cast<int>(DialogViewID::PAY_BUTTON))
                  ->GetEnabled());
  OpenCVCPromptWithCVC(base::ASCIIToUTF16(""));
  views::View* done_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON));
  EXPECT_FALSE(done_button->GetEnabled());

  views::Textfield* cvc_field =
      static_cast<views::Textfield*>(dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::CVC_PROMPT_TEXT_FIELD)));
  cvc_field->SetText(base::UTF8ToUTF16(""));
  cvc_field->InsertOrReplaceText(base::UTF8ToUTF16("0"));
  EXPECT_FALSE(done_button->GetEnabled());

  cvc_field->SetText(base::UTF8ToUTF16(""));
  cvc_field->InsertOrReplaceText(base::UTF8ToUTF16("aaa"));
  EXPECT_FALSE(done_button->GetEnabled());

  cvc_field->SetText(base::UTF8ToUTF16(""));
  cvc_field->InsertOrReplaceText(base::UTF8ToUTF16("111"));
  EXPECT_TRUE(done_button->GetEnabled());
}

}  // namespace payments
