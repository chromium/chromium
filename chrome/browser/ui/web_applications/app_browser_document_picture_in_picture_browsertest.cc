// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Helper class to wait for widget bound changes. Stops waiting once widget size
// passes the given `size_ready_callback` check.
class WidgetResizeWaiter : public views::WidgetObserver {
 public:
  // Should return true if the given size should end waiting.
  using SizeReadyCallback = base::RepeatingCallback<bool(const gfx::Size&)>;

  WidgetResizeWaiter(views::Widget* widget,
                     SizeReadyCallback size_ready_callback)
      : size_ready_callback_(std::move(size_ready_callback)) {
    observation_.Observe(widget);
  }

  void Wait() { run_loop_.Run(); }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override {
    if (size_ready_callback_.Run(bounds.size())) {
      run_loop_.Quit();
    }
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  SizeReadyCallback size_ready_callback_;
  base::RunLoop run_loop_;
};

class AppBrowserDocumentPictureInPictureBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  AppBrowserDocumentPictureInPictureBrowserTest() = default;
  AppBrowserDocumentPictureInPictureBrowserTest(
      const AppBrowserDocumentPictureInPictureBrowserTest&) = delete;
  AppBrowserDocumentPictureInPictureBrowserTest& operator=(
      const AppBrowserDocumentPictureInPictureBrowserTest&) = delete;
  ~AppBrowserDocumentPictureInPictureBrowserTest() override = default;

 protected:
  DocumentPictureInPictureMixinTestBase picture_in_picture_mixin_test_base_{
      &mixin_host_};

  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
};

// Runs Document PiP from a web app with either the Browser-backed or the
// standalone PiP window.
class AppBrowserDocumentPictureInPictureBackendTest
    : public AppBrowserDocumentPictureInPictureBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  AppBrowserDocumentPictureInPictureBackendTest() {
    feature_list_.InitWithFeatureState(features::kDocumentPipStandaloneWindow,
                                       standalone_enabled());
  }

 protected:
  bool standalone_enabled() const { return GetParam(); }

  void ExpectPipBackend(content::WebContents* pip_web_contents) {
    auto* host = DocumentPipHost::FromChildWebContents(pip_web_contents);
    auto* pip_browser_view = BrowserView::GetBrowserViewForNativeWindow(
        pip_web_contents->GetTopLevelNativeWindow());
    if (standalone_enabled()) {
      EXPECT_NE(nullptr, host);
      EXPECT_EQ(nullptr, pip_browser_view);
    } else {
      EXPECT_EQ(nullptr, host);
      ASSERT_NE(nullptr, pip_browser_view);
      EXPECT_TRUE(pip_browser_view->GetIsPictureInPictureType());
    }
  }

  views::Widget* GetPipWidget(content::WebContents* pip_web_contents) {
    return views::Widget::GetWidgetForNativeWindow(
        pip_web_contents->GetTopLevelNativeWindow());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppBrowserDocumentPictureInPictureBackendTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

IN_PROC_BROWSER_TEST_P(AppBrowserDocumentPictureInPictureBackendTest,
                       InnerBoundsMatchRequest) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());

  BrowserWindowInterface* browser =
      web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto kInitialPipSize = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, kInitialPipSize);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);
  ExpectPipBackend(pip_web_contents);

  EXPECT_EQ(kInitialPipSize, pip_web_contents->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_P(AppBrowserDocumentPictureInPictureBackendTest,
                       AppWindowWebContentsSizeUnchangedAfterExitPip) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());
  BrowserWindowInterface* browser =
      web_app::LaunchWebAppBrowser(profile(), app_id);

  // Navigate to the Picture-in-Picture URL, enter Picture-in-Picture and
  // remember the app browser WebContents size.
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser);
  auto* app_browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  const auto expected_app_browser_web_contents_size =
      app_browser_view->GetContentsSize();

  // Verify that we have entered Picture-in-Picture.
  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);
  ExpectPipBackend(pip_web_contents);

  // Exit Picture-in-Picture by closing the PiP window.
  views::Widget* pip_widget = GetPipWidget(pip_web_contents);
  ASSERT_NE(nullptr, pip_widget);
  content::WebContentsDestroyedWatcher pip_web_contents_destroyed_watcher(
      pip_web_contents);
  views::test::WidgetDestroyedWaiter pip_widget_destroyed_waiter(pip_widget);
  pip_widget->Close();
  pip_web_contents_destroyed_watcher.Wait();
  pip_widget_destroyed_waiter.Wait();
  EXPECT_FALSE(picture_in_picture_mixin_test_base_.window_controller()
                   ->GetChildWebContents());

  // Verify that the app browser WebContents size has not changed.
  EXPECT_EQ(expected_app_browser_web_contents_size,
            app_browser_view->GetContentsSize());
}

IN_PROC_BROWSER_TEST_P(AppBrowserDocumentPictureInPictureBackendTest,
                       ResizeToRespectsMinimumInnerWindowSize) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());
  BrowserWindowInterface* browser =
      web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto kInitialPipSize = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, kInitialPipSize);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);
  ExpectPipBackend(pip_web_contents);
  EXPECT_EQ(kInitialPipSize, pip_web_contents->GetContainerBounds().size());

  // Resize Pip window to a size smaller than the allowed minimum.
  EXPECT_TRUE(ExecJs(pip_web_contents, "window.resizeTo(50,50);"));

  // TODO(crbug.com/354785208): Replace with `WidgetResizeWaiter` once bug is
  // fixed.
  base::RunLoop().RunUntilIdle();

  // Verify that the minimum inner window size is respected.
  const gfx::Size contents_size = pip_web_contents->GetContainerBounds().size();
  EXPECT_GE(contents_size.width(),
            PictureInPictureWindowManager::GetMinimumInnerWindowSize().width());
  EXPECT_GE(
      contents_size.height(),
      PictureInPictureWindowManager::GetMinimumInnerWindowSize().height());
}

// TODO(https://crbug.com/422947648): This times out on win11-arm64 builders.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_ResizeToRespectsMaximumWindowSize \
  DISABLED_ResizeToRespectsMaximumWindowSize
#else
#define MAYBE_ResizeToRespectsMaximumWindowSize \
  ResizeToRespectsMaximumWindowSize
#endif
IN_PROC_BROWSER_TEST_P(AppBrowserDocumentPictureInPictureBackendTest,
                       MAYBE_ResizeToRespectsMaximumWindowSize) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());

  BrowserWindowInterface* browser =
      web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto kInitialPipSize = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, kInitialPipSize);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);
  ExpectPipBackend(pip_web_contents);
  EXPECT_EQ(kInitialPipSize, pip_web_contents->GetContainerBounds().size());

  views::Widget* pip_widget = GetPipWidget(pip_web_contents);
  ASSERT_NE(nullptr, pip_widget);
  const display::Screen* const screen = display::Screen::Get();
  const display::Display display =
      screen->GetDisplayNearestWindow(pip_widget->GetNativeWindow());

  // Resize Pip window to a size bigger than the allowed maximum and, verify
  // that the maximum window size is respected.
  //
  // Ideally we would like to perform this resize twice, however this is not
  // possible since during the second resize the window bounds do not change,
  // therefore on Mac `OnWidgetBoundsChanged` is never called. This means that
  // we can't reliably wait for the second resize to complete before completing
  // the test.
  const gfx::Size maximum_window_size =
      PictureInPictureWindowManager::GetMaximumWindowSize(display);
  const gfx::Size exceeding_maximum_window_size =
      maximum_window_size + gfx::Size(100, 100);

  const std::string script = base::StrCat(
      {"window.resizeTo(",
       base::NumberToString(exceeding_maximum_window_size.width()), ",",
       base::NumberToString(exceeding_maximum_window_size.height()), ");"});

  {
    WidgetResizeWaiter waiter(
        pip_widget,
        base::BindRepeating(
            [](gfx::Size maximum_window_size, const gfx::Size& size) {
              return size.width() <= maximum_window_size.width() &&
                     size.height() <= maximum_window_size.height();
            },
            maximum_window_size));
    EXPECT_TRUE(ExecJs(pip_web_contents, script));
    waiter.Wait();
  }

  const gfx::Size window_size = pip_widget->GetWindowBoundsInScreen().size();
  EXPECT_LE(window_size.width(), maximum_window_size.width());
  EXPECT_LE(window_size.height(), maximum_window_size.height());
}

}  // namespace
