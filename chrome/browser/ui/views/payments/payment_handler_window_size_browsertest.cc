// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace payments {

class PaymentHandlerWindowSizeTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWindowSizeTest(const PaymentHandlerWindowSizeTest&) = delete;
  PaymentHandlerWindowSizeTest& operator=(const PaymentHandlerWindowSizeTest&) =
      delete;

 protected:
  PaymentHandlerWindowSizeTest()
      : expected_payment_request_dialog_size_(
            gfx::Size(kDialogMinWidth, kDialogHeight)) {}

  ~PaymentHandlerWindowSizeTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

#if BUILDFLAG(IS_OZONE)
    // Wayland does not support resizing the window bounds programmatically,
    // which means these tests cannot function.
    if (ui::OzonePlatform::RunningOnWaylandForTest()) {
      GTEST_SKIP();
    }
#endif

    NavigateTo("/payment_handler.html");
  }

  const gfx::Size expected_payment_request_dialog_size_;

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPaymentRequestMandatoryPaymentAppUi};
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest, ValidateDialogSize) {
  // Add an autofill profile, so [Continue] button is enabled.
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);

  // Install a payment handler which opens a window.
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  // Invoke a payment request and then check the dialog size when payment sheet
  // is shown.
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_EQ(expected_payment_request_dialog_size_,
            dialog_view()->GetPreferredSize());

  // Adjust the expected PH window height based on the browser content height.
  int browser_window_content_height =
      BrowserWindow::FromBrowser(browser())->GetContentsSize().height();
  gfx::Size expected_payment_handler_dialog_size = gfx::Size(
      kPreferredPaymentHandlerDialogWidth,
      std::max(kDialogHeight, std::min(kPreferredPaymentHandlerDialogHeight,
                                       browser_window_content_height)));

  // Click on Pay and check dialog size when payment handler view is shown.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  EXPECT_EQ(expected_payment_handler_dialog_size,
            dialog_view()->GetPreferredSize());

  // The test flakily hangs if we don't close the payment handler dialog.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest, ResizeToPreferredHeight) {
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  // When browser content height >= 500, dialog height is capped at
  // kPreferredPaymentHandlerDialogHeight (500).
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 800));

  test_api(dialog_view()).ResizeToPaymentHandlerSize();

  EXPECT_EQ(gfx::Size(kPreferredPaymentHandlerDialogWidth,
                      kPreferredPaymentHandlerDialogHeight),
            dialog_view()->GetPreferredSize());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest,
                       ResizeToBrowserContentHeight) {
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  // Set window height to 600, which produces expected dialog height equals to
  // actual browser content height.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 600));

  test_api(dialog_view()).ResizeToPaymentHandlerSize();

  int actual_content_height =
      BrowserWindow::FromBrowser(browser())->GetContentsSize().height();
  EXPECT_EQ(
      gfx::Size(kPreferredPaymentHandlerDialogWidth, actual_content_height),
      dialog_view()->GetPreferredSize());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest,
                       ResizeToMinimumDialogHeight) {
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  // Set window height to 350 so browser content height < kDialogHeight (450).
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 350));

  test_api(dialog_view()).ResizeToPaymentHandlerSize();

  EXPECT_EQ(gfx::Size(kPreferredPaymentHandlerDialogWidth, kDialogHeight),
            dialog_view()->GetPreferredSize());
}

// Height of Chrome's header and progress bar above the payment handler
// WebContents.
constexpr int kHeaderAndProgressBarHeight =
    kPaymentHandlerHeaderHeight + kPaymentHandlerProgressBarHeight;

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest, DefaultWindowSize) {
  // Make the browser large enough that the dialog is not shrunk to fit the
  // browser window.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1000, 800));

  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  // Trigger PaymentRequest without a preferred window size, and wait until the
  // payment handler window is opened.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       content::JsReplace(
                                           "launchWithoutWaitForResponse($1)",
                                           payment_method)));
  ASSERT_TRUE(WaitForObservedEvent());

  content::WebContents* payment_handler_web_contents =
      test_api(dialog_view()).GetPaymentHandlerWebContents();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  gfx::Size expected_dialog_size(kPreferredPaymentHandlerDialogWidth,
                                 kPreferredPaymentHandlerDialogHeight);
  // The WebContents is the dialog minus Chrome's header and progress bar.
  gfx::Size expected_web_contents_size(
      expected_dialog_size.width(),
      expected_dialog_size.height() - kHeaderAndProgressBarHeight);

  // The real dialog bounds have the default size.
  EXPECT_EQ(expected_dialog_size, dialog_view()->GetBoundsInScreen().size());
  // The real WebContents bounds have the expected size.
  EXPECT_EQ(expected_web_contents_size,
            payment_handler_web_contents->GetContainerBounds().size());
}

struct CustomSizeTestParams {
  const char* test_name;
  gfx::Size preferred_web_contents_size;
  gfx::Size expected_web_contents_size;
};

class PaymentHandlerOpenWindowCustomSizeTest
    : public PaymentHandlerWindowSizeTest,
      public testing::WithParamInterface<CustomSizeTestParams> {
 private:
  base::test::ScopedFeatureList custom_size_feature_list_{
      blink::features::kPaymentHandlerStaticCustomSize};
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerOpenWindowCustomSizeTest,
                       ResizesToFitPreferredWebContentsSize) {
  const CustomSizeTestParams& params = GetParam();

  // Make the browser large enough that the dialog is not shrunk to fit the
  // browser window.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1920, 1080));

  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  // Trigger PaymentRequest with a preferred window size, and wait until the
  // payment handler window is opened.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "launchWithoutWaitForResponseWithMethods([{"
                    "  supportedMethods: $1,"
                    "  data: {openWindowOptions: {"
                    "    preferredSize: {"
                    "      width: $2,"
                    "      height: $3,"
                    "    },"
                    "  }},"
                    "}])",
                    payment_method, params.preferred_web_contents_size.width(),
                    params.preferred_web_contents_size.height())));
  ASSERT_TRUE(WaitForObservedEvent());

  content::WebContents* payment_handler_web_contents =
      test_api(dialog_view()).GetPaymentHandlerWebContents();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  // The real dialog bounds are the expected WebContents size plus Chrome's
  // header and progress bar.
  EXPECT_EQ(gfx::Size(params.expected_web_contents_size.width(),
                      params.expected_web_contents_size.height() +
                          kHeaderAndProgressBarHeight),
            dialog_view()->GetBoundsInScreen().size());
  // The real WebContents bounds have the expected size.
  EXPECT_EQ(params.expected_web_contents_size,
            payment_handler_web_contents->GetContainerBounds().size());
}

constexpr int kInRangeWebContentWidth =
    kPaymentHandlerMinDialogWidth +
    (kPaymentHandlerMaxDialogWidth - kPaymentHandlerMinDialogWidth) / 2;
constexpr int kInRangeWebContentHeight =
    kPaymentHandlerMinDialogHeight +
    (kPaymentHandlerMaxDialogHeight - kPaymentHandlerMinDialogHeight) / 2 -
    kHeaderAndProgressBarHeight;

constexpr CustomSizeTestParams kCustomSizeTestParams[] = {
    // Within the [min, max] bounds and already a snapped width.
    {.test_name = "InRangeSizeIsUnchanged",
     .preferred_web_contents_size =
         gfx::Size(kInRangeWebContentWidth, kInRangeWebContentHeight),
     .expected_web_contents_size =
         gfx::Size(kInRangeWebContentWidth, kInRangeWebContentHeight)},
    {.test_name = "WidthIsSnappedUpToMultipleOf16",
     .preferred_web_contents_size = gfx::Size(601, kInRangeWebContentHeight),
     .expected_web_contents_size = gfx::Size(608, kInRangeWebContentHeight)},
    {.test_name = "WidthBelowMinIsClampedToMinWidth",
     .preferred_web_contents_size =
         gfx::Size(kPaymentHandlerMinDialogWidth - 100,
                   kInRangeWebContentHeight),
     .expected_web_contents_size =
         gfx::Size(kPaymentHandlerMinDialogWidth, kInRangeWebContentHeight)},
    {.test_name = "WidthAboveMaxIsClampedToMaxWidth",
     .preferred_web_contents_size =
         gfx::Size(kPaymentHandlerMaxDialogWidth + 100,
                   kInRangeWebContentHeight),
     .expected_web_contents_size =
         gfx::Size(kPaymentHandlerMaxDialogWidth, kInRangeWebContentHeight)},
    {.test_name = "HeightBelowMinIsClampedToMinHeight",
     .preferred_web_contents_size =
         gfx::Size(kInRangeWebContentWidth,
                   kPaymentHandlerMinDialogHeight - 100),
     .expected_web_contents_size = gfx::Size(
         kInRangeWebContentWidth,
         kPaymentHandlerMinDialogHeight - kHeaderAndProgressBarHeight)},
// TODO(crbug.com/573138234): The ChromeOS and Windows test displays are too
// small for kPaymentHandlerMaxDialogHeight, so the dialog is shrunk to fit
// the browser. Decide whether they need different size bounds.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN)
    {.test_name = "HeightAboveMaxIsClampedToMaxHeight",
     .preferred_web_contents_size =
         gfx::Size(kInRangeWebContentWidth,
                   kPaymentHandlerMaxDialogHeight + 100),
     .expected_web_contents_size = gfx::Size(
         kInRangeWebContentWidth,
         kPaymentHandlerMaxDialogHeight - kHeaderAndProgressBarHeight)},
#endif
    {.test_name = "SizeBelowMinIsClampedToMinSize",
     .preferred_web_contents_size =
         gfx::Size(kPaymentHandlerMinDialogWidth - 100,
                   kPaymentHandlerMinDialogHeight - 100),
     .expected_web_contents_size = gfx::Size(
         kPaymentHandlerMinDialogWidth,
         kPaymentHandlerMinDialogHeight - kHeaderAndProgressBarHeight)},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentHandlerOpenWindowCustomSizeTest,
    testing::ValuesIn(kCustomSizeTestParams),
    [](const testing::TestParamInfo<CustomSizeTestParams>& info) {
      return std::string(info.param.test_name);
    });

class PaymentHandlerOpenWindowCustomSizeShrinkTest
    : public PaymentHandlerWindowSizeTest {
 public:
  PaymentHandlerOpenWindowCustomSizeShrinkTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPaymentHandlerStaticCustomSize,
         features::kPaymentRequestRejectTooSmallWindows},
        /*disabled_features=*/{});
    SetBrowserWindowSizeCheckEnabled();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerOpenWindowCustomSizeShrinkTest,
                       ShrinksToFitBrowserContentArea) {
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 600, 600));

  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  // Trigger PaymentRequest with a preferred window size, and wait until the
  // payment handler window is opened.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("launchWithoutWaitForResponseWithMethods([{"
                                   "  supportedMethods: $1,"
                                   "  data: {openWindowOptions: {"
                                   "    preferredSize: {"
                                   "      width: 800,"
                                   "      height: 800,"
                                   "    },"
                                   "  }},"
                                   "}])",
                                   payment_method)));
  ASSERT_TRUE(WaitForObservedEvent());

  // The real dialog bounds are shrunk to fit in the browser content area.
  gfx::Size browser_content_size =
      GetActiveWebContents()->GetContainerBounds().size();
  gfx::Size dialog_size = dialog_view()->GetBoundsInScreen().size();
  EXPECT_LT(dialog_size.width(), browser_content_size.width());
  EXPECT_LT(dialog_size.height(), browser_content_size.height());
}

}  // namespace payments
