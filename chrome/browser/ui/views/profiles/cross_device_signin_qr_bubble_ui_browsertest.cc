// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Must match the internal name set on the bubble's DialogModel.
constexpr char kBubbleWidgetName[] = "CrossDeviceSigninQrBubbleViews";

// Blocks until `view` has exactly the requested height. Returns false if the
// view is deleted while waiting (e.g. the bubble closed unexpectedly).
class ViewHeightWaiter : public views::ViewObserver {
 public:
  explicit ViewHeightWaiter(views::View* view) { observation_.Observe(view); }

  ~ViewHeightWaiter() override = default;

  [[nodiscard]] bool WaitForHeight(int target_height) {
    target_height_ = target_height;
    if (!IsTargetHeightReached() && observation_.IsObserving()) {
      run_loop_.Run();
    }
    return IsTargetHeightReached();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    if (IsTargetHeightReached()) {
      run_loop_.Quit();
    }
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    observation_.Reset();
    run_loop_.Quit();
  }

 private:
  bool IsTargetHeightReached() const {
    const views::View* view = observation_.GetSource();
    return view && view->height() == target_height_;
  }

  int target_height_ = 0;
  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

}  // namespace

class CrossDeviceSigninQrBubbleUIPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  CrossDeviceSigninQrBubbleUIPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam()) {
    scoped_feature_list_.InitAndEnableFeature(
        switches::kCrossDeviceSigninFromDesktop);
    // The QR code bitmap is downscaled when rendered in the bubble, and on
    // Windows that raster non-deterministically produces one of two outputs
    // differing by a few color levels (max 6 per channel) across the QR area.
    // Allow many pixels to differ, but only by a small amount, so that any
    // real change (text, layout, QR contents) is still caught.
    SetPixelMatchAlgorithm(
        std::make_unique<ui::test::FuzzySkiaGoldMatchingAlgorithm>(
            /*max_different_pixels=*/10000, /*pixel_delta_threshold=*/30));
  }

  ~CrossDeviceSigninQrBubbleUIPixelTest() override = default;

  void ShowUi(const std::string& name) override {
    views::Widget* browser_widget =
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
    if (!browser_widget->IsVisible()) {
      views::test::WidgetVisibleWaiter(browser_widget).Wait();
    }

    SignInWithAccount();

    // Go through the production entry point: the bubble is only shown once
    // its WebView has been auto-resized to its content.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, kBubbleWidgetName);
    SigninViewController::From(browser())->ShowCrossDeviceSigninQrBubble(
        GURL("https://www.google.com/chrome/go-mobile"), base::DoNothing());
    views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(widget);

    views::WebView* web_view = views::AsViewClass<views::WebView>(
        views::ElementTrackerViews::GetInstance()->GetUniqueView(
            kCrossDeviceSigninQrBubbleWebViewElementId,
            views::ElementTrackerViews::GetContextForWidget(widget)));
    ASSERT_TRUE(web_view);
    content::WebContents* web_contents = web_view->GetWebContents();
    ASSERT_TRUE(web_contents);
    content::WaitForLoadStop(web_contents);

    // The bubble hosts a Lit web component which asynchronously fetches the
    // QR code and user info over Mojo after the initial page load. Wait for
    // it to render and for the QR code image to be decoded, otherwise the
    // screenshot may capture a blank or partially painted bubble.
    const std::string js_wait = R"(
      (async () => {
        await customElements.whenDefined('cross-device-signin-qr-bubble-app');
        const app = document.querySelector('cross-device-signin-qr-bubble-app');
        if (!app) {
          return 'cross-device-signin-qr-bubble-app not found';
        }
        for (let i = 0; i < 300; ++i) {
          await app.updateComplete;
          const img = app.shadowRoot.querySelector('#qr-code');
          if (img) {
            await img.decode();
            return true;
          }
          await new Promise(resolve => requestAnimationFrame(resolve));
        }
        return '#qr-code was never rendered';
      })();
    )";
    ASSERT_EQ(true, content::EvalJs(web_contents, js_wait));

    // Rendering the QR code and user info triggers another asynchronous
    // auto-resize of the native WebView. Block until it matches the document.
    const int document_height =
        content::EvalJs(web_contents, "document.documentElement.scrollHeight")
            .ExtractInt();
    ASSERT_TRUE(ViewHeightWaiter(web_view).WaitForHeight(document_height));

    // Make sure a compositor frame at the final size has been produced before
    // the screenshot is taken.
    content::WaitForCopyableViewInWebContents(web_contents);
  }

 private:
  // Kept for the whole test so no animation runs during the screenshot.
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CrossDeviceSigninQrBubbleUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CrossDeviceSigninQrBubbleUIPixelTest,
    testing::ValuesIn(std::vector<PixelTestParam>{
        {.test_suffix = "Regular"},
        {.test_suffix = "DarkTheme", .use_dark_theme = true},
        {.test_suffix = "Rtl", .use_right_to_left_language = true},
    }),
    [](const testing::TestParamInfo<PixelTestParam>& info) {
      return info.param.test_suffix;
    });
