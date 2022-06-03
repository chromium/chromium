// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestEmptyUpdateTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestEmptyUpdateTest(const PaymentRequestEmptyUpdateTest&) = delete;
  PaymentRequestEmptyUpdateTest& operator=(
      const PaymentRequestEmptyUpdateTest&) = delete;

 protected:
  PaymentRequestEmptyUpdateTest() {}
};

IN_PROC_BROWSER_TEST_F(PaymentRequestEmptyUpdateTest, NoCrash) {
  NavigateTo("/payment_request_empty_update_test.html");
  AddAutofillProfile(autofill::test::GetFullProfile());
  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING});

  ClickOnChildInListViewAndWait(
      /* child_index=*/0, /*total_num_children=*/1,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);

  // No crash indicates a successful test.
}

}  // namespace payments
