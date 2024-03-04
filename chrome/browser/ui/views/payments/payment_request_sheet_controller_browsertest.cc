// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace payments {

class PaymentRequestSheetControllerTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestSheetControllerTest() = default;
  ~PaymentRequestSheetControllerTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestSheetControllerTest,
                       ProcessingSpinnerViewAccessibility) {
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

  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  EXPECT_TRUE(dialog_view()->throbber_overlay_for_testing()->GetVisible());
  EXPECT_FALSE(dialog_view()
                   ->throbber_overlay_for_testing()
                   ->GetViewAccessibility()
                   .GetIsIgnored());
  EXPECT_FALSE(dialog_view()
                   ->throbber_overlay_for_testing()
                   ->GetViewAccessibility()
                   .IsLeaf());

  // Wait for the response to settle.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));

  // Call retry to finish the dialog loading state and hide the spinner.
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

  EXPECT_FALSE(dialog_view()->throbber_overlay_for_testing()->GetVisible());
  EXPECT_TRUE(dialog_view()
                  ->throbber_overlay_for_testing()
                  ->GetViewAccessibility()
                  .GetIsIgnored());
  EXPECT_TRUE(dialog_view()
                  ->throbber_overlay_for_testing()
                  ->GetViewAccessibility()
                  .IsLeaf());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestSheetControllerTest,
                       HiddenSheetViewAccessibility) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Expect that the payment request view is accessibility visible.
  views::View* payment_request_view =
      GetByDialogViewID(DialogViewID::PAYMENT_REQUEST_SHEET);
  EXPECT_FALSE(payment_request_view->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(payment_request_view->GetViewAccessibility().IsLeaf());

  OpenContactInfoEditorScreen();

  // Expect that the now hidden payment request view is not accessibility
  // visible, and that the contact info view is.
  EXPECT_TRUE(payment_request_view->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(payment_request_view->GetViewAccessibility().IsLeaf());
  views::View* contact_info_view =
      GetByDialogViewID(DialogViewID::CONTACT_INFO_EDITOR_SHEET);
  EXPECT_FALSE(contact_info_view->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(contact_info_view->GetViewAccessibility().IsLeaf());
}

}  // namespace payments
