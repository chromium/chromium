// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestNoUpdateWithTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestNoUpdateWithTest() {}

  void RunJavaScriptFunctionToOpenPaymentRequestUI(
      const std::string& function_name) {
    ResetEventWaiterForDialogOpened();

    content::WebContents* web_contents = GetActiveWebContents();
    ASSERT_TRUE(content::ExecuteScript(web_contents, function_name + "();"));

    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestNoUpdateWithTest);
};

// A merchant that does not listen to shipping address update events will not
// cause timeouts in UI.
IN_PROC_BROWSER_TEST_F(PaymentRequestNoUpdateWithTest, BuyWithoutListeners) {
  NavigateTo("/payment_request_no_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("buyWithoutListeners");

  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::BACK_NAVIGATION});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  ExpectBodyContains({"freeShipping"});
}

// A merchant that listens to shipping address update events, but does not call
// updateWith() on the event, will not cause timeouts in UI.
IN_PROC_BROWSER_TEST_F(PaymentRequestNoUpdateWithTest,
                       BuyWithoutCallingUpdateWith) {
  NavigateTo("/payment_request_no_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("buyWithoutCallingUpdateWith");

  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::BACK_NAVIGATION});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  ExpectBodyContains({"freeShipping"});
}

// A merchant that invokes updateWith() directly without using a promise will
// not cause timeouts in UI.
IN_PROC_BROWSER_TEST_F(PaymentRequestNoUpdateWithTest, BuyWithoutPromises) {
  NavigateTo("/payment_request_no_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("buyWithoutPromises");

  OpenOrderSummaryScreen();
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  ClickOnBackArrow();

  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::BACK_NAVIGATION});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  OpenOrderSummaryScreen();
  EXPECT_EQ(base::ASCIIToUTF16("$10.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  ClickOnBackArrow();

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  ExpectBodyContains({"updatedShipping"});
}

}  // namespace payments
