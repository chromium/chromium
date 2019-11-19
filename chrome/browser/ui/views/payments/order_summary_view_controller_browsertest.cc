// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestOrderSummaryViewControllerTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestOrderSummaryViewControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestOrderSummaryViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestOrderSummaryViewControllerTest,
                       OrderSummaryReflectsShippingOption) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // In MI state, shipping is $5.00.
  autofill::AutofillProfile michigan = autofill::test::GetFullProfile2();
  michigan.set_use_count(100U);
  AddAutofillProfile(michigan);
  // In CA state, there is free shipping.
  autofill::AutofillProfile california = autofill::test::GetFullProfile();
  california.set_use_count(50U);
  AddAutofillProfile(california);

  InvokePaymentRequestUI();

  OpenOrderSummaryScreen();

  // No address is selected.
  // Verify the expected amounts are shown ('Total', 'Pending Shipping Price'
  // and 'Subtotal', respectively).
  EXPECT_EQ(base::ASCIIToUTF16("USD"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_CURRENCY_LABEL));
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(base::ASCIIToUTF16("$0.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));

  // Go to the shipping address screen and select the first address (MI state).
  ClickOnBackArrow();
  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::BACK_NAVIGATION});
  ClickOnChildInListViewAndWait(
      /* child_index=*/0, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  // Michigan address is selected and has standard shipping.
  std::vector<base::string16> shipping_address_labels = GetProfileLabelValues(
      DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION);
  EXPECT_EQ(base::ASCIIToUTF16("Jane A. Smith"), shipping_address_labels[0]);
  EXPECT_EQ(
      base::ASCIIToUTF16("ACME, 123 Main Street, Unit 1, Greensdale, MI 48838"),
      shipping_address_labels[1]);
  EXPECT_EQ(base::ASCIIToUTF16("+1 310-555-7889"), shipping_address_labels[2]);
  std::vector<base::string16> shipping_option_labels =
      GetShippingOptionLabelValues(
          DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
  EXPECT_EQ(base::ASCIIToUTF16("Standard shipping in US"),
            shipping_option_labels[0]);
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"), shipping_option_labels[1]);

  // Go back to Order Summary screen to see updated totals.
  OpenOrderSummaryScreen();

  // Verify the expected amounts are shown ('Total', 'Standard shipping in US'
  // and 'Subtotal', respectively).
  EXPECT_EQ(base::ASCIIToUTF16("USD"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_CURRENCY_LABEL));
  EXPECT_EQ(base::ASCIIToUTF16("$10.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));

  // Go to the shipping address screen and select the second address (CA state).
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

  // California address is selected and has free shipping.
  shipping_address_labels = GetProfileLabelValues(
      DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION);
  EXPECT_EQ(base::ASCIIToUTF16("John H. Doe"), shipping_address_labels[0]);
  EXPECT_EQ(base::ASCIIToUTF16(
                "Underworld, 666 Erebus St., Apt 8, Elysium, CA 91111"),
            shipping_address_labels[1]);
  EXPECT_EQ(base::ASCIIToUTF16("+1 650-211-1111"), shipping_address_labels[2]);
  shipping_option_labels = GetShippingOptionLabelValues(
      DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
  EXPECT_EQ(base::ASCIIToUTF16("Free shipping in California"),
            shipping_option_labels[0]);
  EXPECT_EQ(base::ASCIIToUTF16("$0.00"), shipping_option_labels[1]);

  // Go back to Order Summary screen to see updated totals.
  OpenOrderSummaryScreen();

  // Verify the expected amounts are shown ('Total',
  // 'Free shipping in California' and 'Subtotal', respectively).
  EXPECT_EQ(base::ASCIIToUTF16("USD"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_CURRENCY_LABEL));
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  EXPECT_EQ(base::ASCIIToUTF16("$0.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));
}

}  // namespace payments
