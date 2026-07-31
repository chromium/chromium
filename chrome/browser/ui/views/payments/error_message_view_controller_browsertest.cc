// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/label.h"

namespace payments {

constexpr std::string_view kPaymentAppOrigin = "payment_app.com";
constexpr std::string_view kMerchantOrigin = "merchant.com";

using PaymentRequestErrorMessageTest = PaymentRequestBrowserTestBase;

// Testing the use of the complete('fail') JS API and the error message.
IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest, CompleteFail) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method_name);

  NavigateTo("/payment_request_fail_complete_test.html");

  // Trigger PaymentRequest. Since there is only one payment app, the payment
  // sheet UI is skipped and the payment handler window opens automatically.
  // When the app confirms and the merchant calls complete('fail'), the error
  // message should be shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                         payment_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(test_api(dialog_view()).throbber_overlay()->GetVisible());

  // The user can only close the dialog at this point.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest,
                       EnterKeyClosesErrorDialog) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method_name);

  NavigateTo("/payment_request_fail_complete_test.html");

  // Trigger PaymentRequest. Since there is only one payment app, the payment
  // sheet UI is skipped and the payment handler window opens automatically.
  // When the app confirms and the merchant calls complete('fail'), the error
  // message should be shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                         payment_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Trigger the 'Enter' accelerator, which should be present and mapped to
  // close the dialog.
  views::View* error_sheet =
      dialog_view()->GetViewByID(static_cast<int>(DialogViewID::ERROR_SHEET));
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  EXPECT_TRUE(error_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest,
                       ContentViewNotScrollable) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method_name);

  NavigateTo("/payment_request_fail_complete_test.html");

  // Trigger PaymentRequest. Since there is only one payment app, the payment
  // sheet UI is skipped and the payment handler window opens automatically.
  // When the app confirms and the merchant calls complete('fail'), the error
  // message should be shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                         payment_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a CONTENT_VIEW, we have to explicitly test the
  // front PaymentHandler view here.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();

  views::View* sheet_view =
      GetChildByDialogViewID(top_view, DialogViewID::ERROR_SHEET);
  // The content view should be within the sheet view.
  EXPECT_NE(nullptr,
            GetChildByDialogViewID(sheet_view, DialogViewID::CONTENT_VIEW));

  // There should be no scroll view.
  EXPECT_EQ(nullptr, GetChildByDialogViewID(
                         top_view, DialogViewID::PAYMENT_SHEET_SCROLL_VIEW));
}

class PaymentRequestErrorMessageMandatoryUiEnabledTest
    : public PaymentRequestErrorMessageTest {
 protected:
  PaymentRequestErrorMessageMandatoryUiEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPaymentRequestMandatoryPaymentAppUi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageMandatoryUiEnabledTest,
                       ErrorMessageContainsOrigins) {
  std::string method_name;
  InstallPaymentApp(std::string(kPaymentAppOrigin),
                    "/payment_handler_sw_error_without_user_interaction.js",
                    &method_name);
  NavigateTo(std::string(kMerchantOrigin), "/payment_handler.html");

  // Trigger PaymentRequest. We expect the error message sheet to be shown
  // because the app rejects the payment immediately before any user interaction
  // occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::LOADING_VIEW_SHOWN,
       // Note: LOADING_VIEW_HIDDEN comes after PAYMENT_HANDLER_WINDOW_OPENED
       // because the loading view is hidden asynchronously.
       DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
       DialogEvent::LOADING_VIEW_HIDDEN, DialogEvent::PAYMENT_HANDLER_TITLE_SET,
       DialogEvent::PROCESSING_SPINNER_HIDDEN,
       DialogEvent::ERROR_MESSAGE_SHOWN});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Verify the error message contains the origins.
  views::View* error_sheet =
      dialog_view()->GetViewByID(static_cast<int>(DialogViewID::ERROR_SHEET));
  ASSERT_NE(nullptr, error_sheet);

  views::View* content_view =
      GetChildByDialogViewID(error_sheet, DialogViewID::CONTENT_VIEW);
  ASSERT_NE(nullptr, content_view);
  ASSERT_EQ(2u, content_view->children().size());

  views::Label* label = static_cast<views::Label*>(content_view->children()[1]);
  std::u16string_view label_text = label->GetText();

  EXPECT_TRUE(label_text.contains(base::ASCIIToUTF16(kMerchantOrigin)));
  EXPECT_TRUE(label_text.contains(base::ASCIIToUTF16(kPaymentAppOrigin)));
}

}  // namespace payments
