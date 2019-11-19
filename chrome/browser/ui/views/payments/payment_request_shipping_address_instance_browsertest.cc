// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace payments {
namespace {

class PaymentRequestShippingAddressInstanceTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestShippingAddressInstanceTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestShippingAddressInstanceTest);
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

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  WaitForObservedEvent();

  ExpectBodyContains({"Same instance: true"});
}

}  // namespace
}  // namespace payments
