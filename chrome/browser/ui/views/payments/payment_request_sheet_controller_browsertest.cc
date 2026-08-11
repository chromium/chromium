// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace payments {

class PaymentRequestSheetControllerTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestSheetControllerTest() = default;
  ~PaymentRequestSheetControllerTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPaymentRequestMandatoryPaymentAppUi};
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

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  EXPECT_EQ(1U, GetPaymentRequests().size());
  EXPECT_TRUE(test_api(dialog_view()).view_stack()->GetVisible());
  EXPECT_EQ(nullptr, test_api(dialog_view()).loading_view_overlay());

  ResetEventWaiter(DialogEvent::PROCESSING_SPINNER_SHOWN);
  dialog_view()->ShowProcessingSpinner();

  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_TRUE(test_api(dialog_view()).throbber_overlay()->GetVisible());
  EXPECT_FALSE(test_api(dialog_view())
                   .throbber_overlay()
                   ->GetViewAccessibility()
                   .GetIsIgnored());
  EXPECT_FALSE(test_api(dialog_view())
                   .throbber_overlay()
                   ->GetViewAccessibility()
                   .IsLeaf());

  ResetEventWaiter(DialogEvent::PROCESSING_SPINNER_HIDDEN);
  dialog_view()->HideProcessingSpinner();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_FALSE(test_api(dialog_view()).throbber_overlay()->GetVisible());
  EXPECT_TRUE(test_api(dialog_view())
                  .throbber_overlay()
                  ->GetViewAccessibility()
                  .GetIsIgnored());
  EXPECT_TRUE(test_api(dialog_view())
                  .throbber_overlay()
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
