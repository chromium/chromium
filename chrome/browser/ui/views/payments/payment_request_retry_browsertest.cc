// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

using PaymentRequestRetryTest = PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestRetryTest,
                       DoNotAllowPaymentInstrumentChange) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_retry_with_payer_errors.html");
  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  AddAutofillProfile(contact);

  // Confirm that there are two payment apps available.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(2U, request->state()->available_apps().size());

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Wait for the response to settle.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));

  // Confirm that only one payment app is available for retry().
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::CONTACT_INFO_EDITOR_OPENED});
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "retry({"
                              "  payer: {"
                              "    email: 'EMAIL ERROR',"
                              "    name: 'NAME ERROR',"
                              "    phone: 'PHONE ERROR'"
                              "  }"
                              "});"));
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_EQ(1U, request->state()->available_apps().size());
}

}  // namespace payments
