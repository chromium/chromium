// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestModifiersTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestModifiersTest(const PaymentRequestModifiersTest&) = delete;
  PaymentRequestModifiersTest& operator=(const PaymentRequestModifiersTest&) =
      delete;

 protected:
  PaymentRequestModifiersTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  size_t GetLineCount() {
    auto* top = dialog_view()->view_stack_for_testing()->top();
    const auto* content =
        top->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW));
    return content->children().size();
  }
};

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       NoModifierAppliedIfNoSelectedInstrument) {
  // We need to control the default-selected payment app in the UI, to know that
  // the modifiers should not apply. To achieve this, we install one of the apps
  // without an icon - this makes ServiceWorkerPaymentApp::CanPreselect false
  // for it and so the other app should be selected.
  std::string payment_method_name_1;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name_1);
  std::string payment_method_name_2;
  InstallPaymentAppWithoutIcon("b.com", "/payment_request_success_responder.js",
                               &payment_method_name_2);

  NavigateTo("/payment_request_with_modifiers_test.html");

  InvokePaymentRequestUIWithJs(
      content::JsReplace("modifierToSecondaryMethod([{supportedMethods:$1}, "
                         "{supportedMethods:$2}]);",
                         payment_method_name_1, payment_method_name_2));

  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1u, GetLineCount());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       NoTotalInModifierDoesNotCrash) {
  // The modifier without the total applies to the first method; to make sure
  // that it is always selected we install the second method without an icon.
  std::string payment_method_name_1;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name_1);
  std::string payment_method_name_2;
  InstallPaymentAppWithoutIcon("b.com", "/payment_request_success_responder.js",
                               &payment_method_name_2);

  NavigateTo("/payment_request_with_modifiers_test.html");

  InvokePaymentRequestUIWithJs(content::JsReplace(
      "modifierWithNoTotal([{supportedMethods:$1}, {supportedMethods:$2}]);",
      payment_method_name_1, payment_method_name_2));

  OpenOrderSummaryScreen();

  // The price is the global total, because the modifier does not have total.
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // Only global total is available.
  EXPECT_EQ(1u, GetLineCount());
}

}  // namespace payments
