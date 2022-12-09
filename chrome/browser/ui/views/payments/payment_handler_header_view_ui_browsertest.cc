// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentHandlerHeaderViewUITest
    : public PaymentRequestBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PaymentHandlerHeaderViewUITest() : minimal_header_ux_enabled_(GetParam()) {
    if (minimal_header_ux_enabled_) {
      features_.InitAndEnableFeature(features::kPaymentHandlerMinimalHeaderUX);
    } else {
      features_.InitAndDisableFeature(features::kPaymentHandlerMinimalHeaderUX);
    }
  }
  ~PaymentHandlerHeaderViewUITest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }

 protected:
  bool minimal_header_ux_enabled_;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerHeaderViewUITest,
                       HeaderHasCorrectDetails) {
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
  WaitForObservedEvent();

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a SHEET_TITLE, we have to explicitly test the
  // front PaymentHandler view here.
  ViewStack* view_stack = dialog_view()->view_stack_for_testing();

  if (minimal_header_ux_enabled_) {
    EXPECT_TRUE(IsViewVisible(DialogViewID::CANCEL_BUTTON, view_stack->top()));
    EXPECT_FALSE(IsViewVisible(DialogViewID::BACK_BUTTON, view_stack->top()));
  } else {
    EXPECT_TRUE(IsViewVisible(DialogViewID::BACK_BUTTON, view_stack->top()));
    EXPECT_FALSE(IsViewVisible(DialogViewID::CANCEL_BUTTON, view_stack->top()));
  }
  EXPECT_TRUE(IsViewVisible(DialogViewID::SHEET_TITLE, view_stack->top()));
  EXPECT_TRUE(
      IsViewVisible(DialogViewID::PAYMENT_APP_HEADER_ICON, view_stack->top()));
  EXPECT_TRUE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET,
                            view_stack->top()));

  if (minimal_header_ux_enabled_) {
    // In the minimal header UX, only the origin is shown and is marked as the
    // title. For this test, the origin can be derived from the method name.
    ASSERT_TRUE(base::StartsWith(method_name, "https://"));
    EXPECT_EQ(base::ASCIIToUTF16(method_name.substr(8)),
              GetLabelText(DialogViewID::SHEET_TITLE, view_stack->top()));
  } else {
    // This page has a <title>, and so should show the sheet title rather than
    // the origin as the title.
    EXPECT_EQ(u"Payment App",
              GetLabelText(DialogViewID::SHEET_TITLE, view_stack->top()));
  }
}

IN_PROC_BROWSER_TEST_P(PaymentHandlerHeaderViewUITest, HeaderWithoutIcon) {
  // TODO(crbug.com/1385136): Handle missing/empty icons in minimal header UX.
  if (minimal_header_ux_enabled_)
    return;

  std::string method_name;
  InstallPaymentAppWithoutIcon("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest. Since the Payment App has no icon this will show
  // the browser sheet first, and we have to manually select the payment app to
  // continue.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  WaitForObservedEvent();

  // Select the installed payment app.
  OpenPaymentMethodScreen();
  ResetEventWaiter(DialogEvent::BACK_NAVIGATION);
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  ASSERT_TRUE(list_view);
  EXPECT_EQ(1u, list_view->children().size());
  ClickOnDialogViewAndWait(list_view->children()[0]);

  // The pay button should be enabled now.
  ASSERT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON);

  // The payment app has no icon, so it should not be displayed on the header.
  EXPECT_FALSE(IsViewVisible(DialogViewID::PAYMENT_APP_HEADER_ICON));
}

INSTANTIATE_TEST_SUITE_P(All, PaymentHandlerHeaderViewUITest, testing::Bool());

}  // namespace
}  // namespace payments
