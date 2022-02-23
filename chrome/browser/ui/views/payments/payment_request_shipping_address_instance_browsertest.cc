// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace payments {
namespace {

class PaymentRequestShippingAddressInstanceTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestShippingAddressInstanceTest(
      const PaymentRequestShippingAddressInstanceTest&) = delete;
  PaymentRequestShippingAddressInstanceTest& operator=(
      const PaymentRequestShippingAddressInstanceTest&) = delete;

 protected:
  PaymentRequestShippingAddressInstanceTest() {
    feature_list_.InitWithFeatures({::features::kPaymentRequestBasicCard}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressInstanceTest,
                       ShouldBeSameInstance) {
  NavigateTo("/payment_request_shipping_address_instance_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  PayWithCreditCardAndWait(u"123");

  WaitForObservedEvent();

  ExpectBodyContains({"Same instance: true"});
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestShippingAddressInstanceTest, but with basic-card disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestShippingAddressInstanceBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestShippingAddressInstanceBasicCardDisabledTest(
      const PaymentRequestShippingAddressInstanceBasicCardDisabledTest&) =
      delete;
  PaymentRequestShippingAddressInstanceBasicCardDisabledTest& operator=(
      const PaymentRequestShippingAddressInstanceBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestShippingAddressInstanceBasicCardDisabledTest() {
    feature_list_.InitWithFeatures({}, {::features::kPaymentRequestBasicCard});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestShippingAddressInstanceBasicCardDisabledTest,
    ShouldBeSameInstance) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &payment_method_name);

  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);

  NavigateTo("/payment_request_shipping_address_instance_test.html");

  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");

  // The PaymentRequest main UI should be showing, as a shipping address was
  // requested. Click on the 'Pay' button and wait for the PaymentHandler to
  // (automatically) handle the payment.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForObservedEvent();

  // Verify that the shippingAddress instance in the request and response were
  // the same object instance.
  ExpectBodyContains({"Same instance: true"});
}

}  // namespace
}  // namespace payments
