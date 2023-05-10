// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestUpdateWithTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestUpdateWithTest(const PaymentRequestUpdateWithTest&) = delete;
  PaymentRequestUpdateWithTest& operator=(const PaymentRequestUpdateWithTest&) =
      delete;

 protected:
  PaymentRequestUpdateWithTest() = default;

  void RunJavaScriptFunctionToOpenPaymentRequestUI(
      const std::string& function_name,
      const std::string& payment_method_name) {
    ResetEventWaiterForDialogOpened();

    content::WebContents* web_contents = GetActiveWebContents();
    ASSERT_TRUE(content::ExecJs(
        web_contents, function_name + "('" + payment_method_name + "');"));

    ASSERT_TRUE(WaitForObservedEvent());
  }
};

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest, UpdateWithEmpty) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_update_with_test.html");
  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithEmpty",
                                              payment_method_name);

  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
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
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
  ClickOnBackArrow();

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest, UpdateWithTotal) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_update_with_test.html");
  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithTotal",
                                              payment_method_name);

  OpenOrderSummaryScreen();
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
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
  EXPECT_EQ(u"$10.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
  ClickOnBackArrow();

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest, UpdateWithDisplayItems) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_update_with_test.html");
  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithDisplayItems",
                                              payment_method_name);

  OpenOrderSummaryScreen();
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
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
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
  ClickOnBackArrow();

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest,
                       UpdateWithShippingOptions) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_update_with_test.html");
  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithShippingOptions",
                                              payment_method_name);

  OpenOrderSummaryScreen();
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
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
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
  ClickOnBackArrow();

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  ExpectBodyContains({"updatedShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest, UpdateWithModifiers) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_update_with_test.html");
  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithModifiers",
                                              payment_method_name);

  OpenOrderSummaryScreen();
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"$0.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
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
  EXPECT_EQ(u"$4.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(u"$2.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$3.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
  EXPECT_EQ(u"-$1.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_3));
  ClickOnBackArrow();

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  ExpectBodyContains({"freeShipping"});
}

// Show the shipping address validation error message even if the merchant
// provided some shipping options.
IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest, UpdateWithError) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_update_with_test.html");
  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithError",
                                              payment_method_name);

  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  EXPECT_EQ(u"This is an error for a browsertest",
            GetLabelText(DialogViewID::WARNING_LABEL));
}

}  // namespace payments
