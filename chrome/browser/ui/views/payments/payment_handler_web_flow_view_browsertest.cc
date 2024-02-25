// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/scroll_view.h"

namespace payments {

using PaymentHandlerWebFlowViewTest = PaymentRequestBrowserTestBase;

// Test that the content view itself is not in a ScrollView, as the web view
// should be a static size that is itself scrollable.
IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewTest,
                       ContentViewNotScrollable) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest, and wait until the PaymentHandler has loaded a
  // web-contents that has set a title.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a CONTENT_VIEW, we have to explicitly test the
  // front PaymentHandler view here.
  views::View* top_view = dialog_view()->view_stack_for_testing()->top();

  views::View* sheet_view = GetChildByDialogViewID(
      top_view, DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET);
  // The content view should be within the sheet view.
  EXPECT_NE(nullptr,
            GetChildByDialogViewID(sheet_view, DialogViewID::CONTENT_VIEW));

  // There should be no scroll view.
  EXPECT_EQ(nullptr, GetChildByDialogViewID(
                         top_view, DialogViewID::PAYMENT_SHEET_SCROLL_VIEW));
}

}  // namespace payments
