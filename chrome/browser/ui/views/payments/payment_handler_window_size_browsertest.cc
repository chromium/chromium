// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>
#include <string>

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
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

// The payment handler dialog is shrunk to fit the browser if the browser
// content area is smaller than the dialog size scaled by this ratio. Must match
// kShrinkToBrowserRatio in payment_request_dialog_view.cc.
constexpr float kShrinkToBrowserRatio = 1.15f;

constexpr char kBrowserContentAreaTooSmallMessage[] =
    "The browser content area is too small to show the payment handler dialog "
    "without shrinking it.";

// Base class for tests of custom Payment Handler window sizes, which open the
// payment handler window directly and check its size.
class PaymentHandlerCustomSizeTest : public PaymentHandlerWindowSizeTest {
 protected:
  // Returns whether the browser content area is too small to show a payment
  // handler dialog of the max size without shrinking it to fit the browser.
  // This happens when the test display is too small for the requested browser
  // bounds, e.g. on some macOS and ChromeOS bots (crbug.com/573138234). Callers
  // should GTEST_SKIP() in that case.
  bool IsBrowserContentAreaTooSmallForTheTest() {
    const gfx::Size dialog_size(kPaymentHandlerMaxDialogWidth,
                                kPaymentHandlerMaxDialogHeight);
    // Mirrors PaymentRequestDialogView::ResizeToFitPaymentHandlerWebContents().
    const gfx::Size browser_content_area =
        GetActiveWebContents()->GetContainerBounds().size();
    const gfx::Size max_dialog_size = gfx::ScaleToFlooredSize(
        browser_content_area, 1.0f / kShrinkToBrowserRatio);
    if (dialog_size.width() <= max_dialog_size.width() &&
        dialog_size.height() <= max_dialog_size.height()) {
      return false;
    }
    LOG(WARNING) << "Browser content area " << browser_content_area.ToString()
                 << " is too small for dialog size " << dialog_size.ToString();
    return true;
  }

  // Installs a payment app and opens its payment handler window, with
  // openWindow({preferredSize}) if `preferred_web_contents_size` is set.
  // Returns the payment handler WebContents, or nullptr on failure.
  content::WebContents* OpenPaymentHandlerWindow(
      const std::optional<gfx::Size>& preferred_web_contents_size =
          std::nullopt) {
    std::string payment_method;
    InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::DIALOG_OPENED,
                                 DialogEvent::LOADING_VIEW_SHOWN,
                                 DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                                 DialogEvent::LOADING_VIEW_HIDDEN,
                                 DialogEvent::PAYMENT_HANDLER_TITLE_SET});
    const std::string script =
        preferred_web_contents_size.has_value()
            ? content::JsReplace(
                  "launchWithoutWaitForResponseWithMethods([{"
                  "  supportedMethods: $1,"
                  "  data: {openWindowOptions: {"
                  "    preferredSize: {"
                  "      width: $2,"
                  "      height: $3,"
                  "    },"
                  "  }},"
                  "}])",
                  payment_method, preferred_web_contents_size->width(),
                  preferred_web_contents_size->height())
            : content::JsReplace("launchWithoutWaitForResponse($1)",
                                 payment_method);
    // ASSERT_* cannot be used in a function that returns a value, so use
    // EXPECT_* and return early on failure.
    EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), script));
    EXPECT_TRUE(WaitForObservedEvent());
    if (HasFailure()) {
      return nullptr;
    }

    return test_api(dialog_view()).GetPaymentHandlerWebContents();
  }

  // Expects that the payment handler page sees `expected_web_contents_size` as
  // its [innerWidth, innerHeight], and that the dialog is that size plus
  // Chrome's header and progress bar.
  void ExpectPaymentHandlerSize(
      content::WebContents* payment_handler_web_contents,
      const gfx::Size& expected_web_contents_size) {
    EXPECT_EQ(gfx::Size(expected_web_contents_size.width(),
                        expected_web_contents_size.height() +
                            kHeaderAndProgressBarHeight),
              dialog_view()->GetBoundsInScreen().size());
    content::EvalJsResult inner_size = content::EvalJs(
        payment_handler_web_contents, "[innerWidth, innerHeight]");
    ASSERT_TRUE(inner_size.is_ok()) << inner_size;
    const base::ListValue& list = inner_size.ExtractList();
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(expected_web_contents_size,
              gfx::Size(list[0].GetInt(), list[1].GetInt()));
  }
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerCustomSizeTest, DefaultWindowSize) {
  // Make the browser large enough that the dialog is not shrunk to fit the
  // browser window.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1920, 1080));
  if (IsBrowserContentAreaTooSmallForTheTest()) {
    GTEST_SKIP() << kBrowserContentAreaTooSmallMessage;
  }

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  // The dialog has the default size, and the WebContents is the dialog minus
  // Chrome's header and progress bar.
  ExpectPaymentHandlerSize(payment_handler_web_contents,
                           gfx::Size(kPreferredPaymentHandlerDialogWidth,
                                     kPreferredPaymentHandlerDialogHeight -
                                         kHeaderAndProgressBarHeight));
}

struct CustomSizeTestParams {
  const char* test_name;
  gfx::Size preferred_web_contents_size;
  gfx::Size expected_web_contents_size;
};

class PaymentHandlerOpenWindowCustomSizeTest
    : public PaymentHandlerCustomSizeTest,
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

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow(params.preferred_web_contents_size);
  ASSERT_NE(nullptr, payment_handler_web_contents);

  ExpectPaymentHandlerSize(payment_handler_web_contents,
                           params.expected_web_contents_size);
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
// TODO(crbug.com/573138234): The ChromeOS, Windows, and Mac test displays
// are too small for kPaymentHandlerMaxDialogHeight, so the dialog is shrunk
// to fit the browser. Decide whether they need different size bounds.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
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
    : public PaymentHandlerCustomSizeTest {
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

  ASSERT_NE(nullptr, OpenPaymentHandlerWindow(gfx::Size(800, 800)));

  // The real dialog bounds are shrunk to fit in the browser content area.
  gfx::Size browser_content_size =
      GetActiveWebContents()->GetContainerBounds().size();
  gfx::Size dialog_size = dialog_view()->GetBoundsInScreen().size();
  EXPECT_LT(dialog_size.width(), browser_content_size.width());
  EXPECT_LT(dialog_size.height(), browser_content_size.height());
}

class PaymentHandlerResizeToTest : public PaymentHandlerCustomSizeTest {
 protected:
  // Calls window.resizeTo() in the payment handler window and waits for the
  // resulting `resize` event.
  void ResizeToAndWait(content::WebContents* payment_handler_web_contents,
                       const gfx::Size& size) {
    EXPECT_TRUE(content::ExecJs(
        payment_handler_web_contents,
        content::JsReplace(
            "new Promise(resolve => {"
            "  addEventListener('resize', resolve, {once: true});"
            "  resizeTo($1, $2);"
            "})",
            size.width(), size.height())));
  }

 private:
  base::test::ScopedFeatureList dynamic_size_feature_list_{
      features::kPaymentHandlerDynamicCustomSize};
};

class PaymentHandlerResizeToCustomSizeTest
    : public PaymentHandlerResizeToTest,
      public testing::WithParamInterface<CustomSizeTestParams> {};

IN_PROC_BROWSER_TEST_P(PaymentHandlerResizeToCustomSizeTest,
                       ResizesToFitRequestedWebContentsSize) {
  const CustomSizeTestParams& params = GetParam();

  // Make the browser large enough that the dialog is not shrunk to fit the
  // browser window.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1920, 1080));

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  ResizeToAndWait(payment_handler_web_contents,
                  params.preferred_web_contents_size);
  ExpectPaymentHandlerSize(payment_handler_web_contents,
                           params.expected_web_contents_size);
}

// The same size bounds apply to openWindow() and resizeTo(). None of the
// expected sizes is the default payment handler size, so each resizeTo() call
// fires a `resize` event.
INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentHandlerResizeToCustomSizeTest,
    testing::ValuesIn(kCustomSizeTestParams),
    [](const testing::TestParamInfo<CustomSizeTestParams>& info) {
      return std::string(info.param.test_name);
    });

IN_PROC_BROWSER_TEST_F(PaymentHandlerResizeToTest, ResizeToRepeatedly) {
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1920, 1080));
  if (IsBrowserContentAreaTooSmallForTheTest()) {
    GTEST_SKIP() << kBrowserContentAreaTooSmallMessage;
  }

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  const gfx::Size kTallSize(kInRangeWebContentWidth, 500);
  const gfx::Size kShortSize(kInRangeWebContentWidth, 250);
  const gfx::Size kWideSize(800, kInRangeWebContentHeight);

  ResizeToAndWait(payment_handler_web_contents, kTallSize);
  ExpectPaymentHandlerSize(payment_handler_web_contents, kTallSize);

  ResizeToAndWait(payment_handler_web_contents, kShortSize);
  ExpectPaymentHandlerSize(payment_handler_web_contents, kShortSize);

  ResizeToAndWait(payment_handler_web_contents, kWideSize);
  ExpectPaymentHandlerSize(payment_handler_web_contents, kWideSize);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerResizeToTest,
                       ResizeToIgnoredWhileProcessingSpinnerIsShown) {
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1920, 1080));
  if (IsBrowserContentAreaTooSmallForTheTest()) {
    GTEST_SKIP() << kBrowserContentAreaTooSmallMessage;
  }

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  // Cover the payment handler window with the processing spinner, as after the
  // payment app responds and before the merchant calls complete().
  dialog_view()->ShowProcessingSpinner();
  ASSERT_FALSE(dialog_view()->IsShowingInteractivePaymentHandlerWindow());

  EXPECT_TRUE(content::ExecJs(
      payment_handler_web_contents,
      content::JsReplace("resizeTo($1, $2);", kInRangeWebContentWidth,
                         kInRangeWebContentHeight)));

  // The payment handler window keeps its default size.
  ExpectPaymentHandlerSize(payment_handler_web_contents,
                           gfx::Size(kPreferredPaymentHandlerDialogWidth,
                                     kPreferredPaymentHandlerDialogHeight -
                                         kHeaderAndProgressBarHeight));
}

class PaymentHandlerResizeToShrinkTest : public PaymentHandlerResizeToTest {
 public:
  PaymentHandlerResizeToShrinkTest() {
    // Browser tests skip the too-small-window check unless it is enabled.
    SetBrowserWindowSizeCheckEnabled();
  }

 private:
  base::test::ScopedFeatureList reject_too_small_feature_list_{
      features::kPaymentRequestRejectTooSmallWindows};
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerResizeToShrinkTest,
                       ShrinksToFitBrowserContentArea) {
  // The default size fits in this browser, but the maximum size doesn't.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1000, 800));

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow();
  ASSERT_NE(nullptr, payment_handler_web_contents);
  ResizeToAndWait(
      payment_handler_web_contents,
      gfx::Size(kPaymentHandlerMaxDialogWidth, kPaymentHandlerMaxDialogHeight));

  // The dialog is shrunk to fit in the browser content area, and is still
  // showing the payment handler.
  gfx::Size browser_content_size =
      GetActiveWebContents()->GetContainerBounds().size();
  gfx::Size dialog_size = dialog_view()->GetBoundsInScreen().size();
  EXPECT_LT(dialog_size.height(), kPaymentHandlerMaxDialogHeight);
  EXPECT_LT(dialog_size.width(), browser_content_size.width());
  EXPECT_LT(dialog_size.height(), browser_content_size.height());
  EXPECT_EQ(payment_handler_web_contents,
            test_api(dialog_view()).GetPaymentHandlerWebContents());
}

class PaymentHandlerResizeToDisabledTest : public PaymentHandlerCustomSizeTest {
 public:
  PaymentHandlerResizeToDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kPaymentHandlerDynamicCustomSize);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerResizeToDisabledTest, ResizeToIsIgnored) {
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 1920, 1080));
  if (IsBrowserContentAreaTooSmallForTheTest()) {
    GTEST_SKIP() << kBrowserContentAreaTooSmallMessage;
  }

  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow();
  ASSERT_NE(nullptr, payment_handler_web_contents);
  const gfx::Size dialog_size = dialog_view()->GetBoundsInScreen().size();
  const gfx::Size web_contents_size =
      payment_handler_web_contents->GetContainerBounds().size();

  // Call resizeTo() and wait two frames.
  ASSERT_TRUE(content::ExecJs(payment_handler_web_contents,
                              "resizeTo(800, 300);"
                              "new Promise(r => requestAnimationFrame(() => "
                              "requestAnimationFrame(r)))"));

  EXPECT_EQ(dialog_size, dialog_view()->GetBoundsInScreen().size());
  EXPECT_EQ(web_contents_size,
            payment_handler_web_contents->GetContainerBounds().size());
}

}  // namespace payments
