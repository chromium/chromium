// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestEmptyUpdateTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestEmptyUpdateTest(const PaymentRequestEmptyUpdateTest&) = delete;
  PaymentRequestEmptyUpdateTest& operator=(
      const PaymentRequestEmptyUpdateTest&) = delete;

 protected:
  PaymentRequestEmptyUpdateTest() {
    feature_list_.InitAndEnableFeature(features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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

// The tests in this class correspond to the tests of the same name in
// PaymentRequestEmptyUpdateTest, with the basic-card being disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestEmptyUpdateBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestEmptyUpdateBasicCardDisabledTest(
      const PaymentRequestEmptyUpdateBasicCardDisabledTest&) = delete;
  PaymentRequestEmptyUpdateBasicCardDisabledTest& operator=(
      const PaymentRequestEmptyUpdateBasicCardDisabledTest&) = delete;

 protected:
  PaymentRequestEmptyUpdateBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestEmptyUpdateBasicCardDisabledTest,
                       NoCrash) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_empty_update_test.html");
  AddAutofillProfile(autofill::test::GetFullProfile());
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
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
