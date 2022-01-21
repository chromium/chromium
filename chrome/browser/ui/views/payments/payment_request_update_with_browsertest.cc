// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestUpdateWithTestBase : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestUpdateWithTestBase(const PaymentRequestUpdateWithTestBase&) =
      delete;
  PaymentRequestUpdateWithTestBase& operator=(
      const PaymentRequestUpdateWithTestBase&) = delete;

 protected:
  PaymentRequestUpdateWithTestBase() = default;

  void RunJavaScriptFunctionToOpenPaymentRequestUI(
      const std::string& function_name,
      const std::string& payment_method_name) {
    ResetEventWaiterForDialogOpened();

    content::WebContents* web_contents = GetActiveWebContents();
    ASSERT_TRUE(content::ExecuteScript(
        web_contents, function_name + "('" + payment_method_name + "');"));

    WaitForObservedEvent();
  }
};

class PaymentRequestUpdateWithBasicCardEnabledTest
    : public PaymentRequestUpdateWithTestBase {
 public:
  PaymentRequestUpdateWithBasicCardEnabledTest(
      const PaymentRequestUpdateWithBasicCardEnabledTest&) = delete;
  PaymentRequestUpdateWithBasicCardEnabledTest& operator=(
      const PaymentRequestUpdateWithBasicCardEnabledTest&) = delete;

 protected:
  PaymentRequestUpdateWithBasicCardEnabledTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithBasicCardEnabledTest,
                       UpdateWithEmpty) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithEmpty", "basic-card");

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

  PayWithCreditCardAndWait(u"123");

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithBasicCardEnabledTest,
                       UpdateWithTotal) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithTotal", "basic-card");

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

  PayWithCreditCardAndWait(u"123");

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithBasicCardEnabledTest,
                       UpdateWithDisplayItems) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithDisplayItems",
                                              "basic-card");

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

  PayWithCreditCardAndWait(u"123");

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithBasicCardEnabledTest,
                       UpdateWithShippingOptions) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithShippingOptions",
                                              "basic-card");

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

  PayWithCreditCardAndWait(u"123");

  ExpectBodyContains({"updatedShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithBasicCardEnabledTest,
                       UpdateWithModifiers) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithModifiers",
                                              "basic-card");

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

  PayWithCreditCardAndWait(u"123");

  ExpectBodyContains({"freeShipping"});
}

// Show the shipping address validation error message even if the merchant
// provided some shipping options.
IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithBasicCardEnabledTest,
                       UpdateWithError) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithError", "basic-card");

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

// The tests in this class correspond to the tests of the same name in
// PaymentRequestUpdateWithTestBase, with the basic-card being disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestUpdateWithWithBasicCardDisabledTest
    : public PaymentRequestUpdateWithTestBase {
 public:
  PaymentRequestUpdateWithWithBasicCardDisabledTest(
      const PaymentRequestUpdateWithWithBasicCardDisabledTest&) = delete;
  PaymentRequestUpdateWithWithBasicCardDisabledTest& operator=(
      const PaymentRequestUpdateWithWithBasicCardDisabledTest&) = delete;

 protected:
  PaymentRequestUpdateWithWithBasicCardDisabledTest() {
    feature_list_.InitWithFeatures({}, {::features::kPaymentRequestBasicCard});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithWithBasicCardDisabledTest,
                       UpdateWithEmpty) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithWithBasicCardDisabledTest,
                       UpdateWithTotal) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithWithBasicCardDisabledTest,
                       UpdateWithDisplayItems) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithWithBasicCardDisabledTest,
                       UpdateWithShippingOptions) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithWithBasicCardDisabledTest,
                       UpdateWithModifiers) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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
IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithWithBasicCardDisabledTest,
                       UpdateWithError) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());

  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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
