// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

using PaymentRequestShippingOptionViewControllerTest =
    PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestShippingOptionViewControllerTest,
                       SelectingVariousShippingOptions) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // In MI state, shipping is $5.00.
  autofill::AutofillProfile michigan = autofill::test::GetFullProfile2();
  michigan.set_use_count(100U);
  AddAutofillProfile(michigan);
  // A Canadian address will have no shipping options.
  autofill::AutofillProfile canada = autofill::test::GetFullProfile();
  canada.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY, u"CA");
  canada.set_use_count(50U);
  AddAutofillProfile(canada);

  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  // There is no shipping option section, because no address has been selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(2U, request->state()->shipping_profiles().size());
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON)));

  // Go to the shipping address screen and select the first address (MI state).
  OpenShippingAddressSectionScreen();
  EXPECT_EQ(u"To see shipping methods and requirements, select an address",
            GetLabelText(DialogViewID::WARNING_LABEL));

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
  std::vector<std::u16string> shipping_address_labels = GetProfileLabelValues(
      DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION);
  EXPECT_EQ(u"Jane A. Smith", shipping_address_labels[0]);
  EXPECT_EQ(u"ACME, 123 Main Street, Unit 1, Greensdale, MI 48838",
            shipping_address_labels[1]);
  EXPECT_EQ(u"+1 310-555-7889", shipping_address_labels[2]);

  // The shipping option section exists, and the shipping option is shown.
  std::vector<std::u16string> shipping_option_labels =
      GetShippingOptionLabelValues(
          DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
  EXPECT_EQ(u"Standard shipping in US", shipping_option_labels[0]);
  EXPECT_EQ(u"$5.00", shipping_option_labels[1]);

  // Go to the shipping address screen and select the second address (Canada).
  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW, false);

  // Now no address is selected.
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());
  EXPECT_EQ(request->state()->shipping_profiles().back(),
            request->state()->selected_shipping_option_error_profile());

  // The address selector has this error.
  EXPECT_EQ(u"We do not ship to this address",
            GetLabelText(DialogViewID::WARNING_LABEL));

  // There is no a longer shipping option section, because no shipping options
  // are available for Canada.
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON)));
}

}  // namespace payments
