// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Helper class to wait for widget bound changes. Stops waiting once widget size
// matches the `expected_size_`.
class WidgetResizeWaiter : public views::WidgetObserver {
 public:
  explicit WidgetResizeWaiter(views::Widget* widget, gfx::Size expected_size)
      : expected_size_(expected_size) {
    observation_.Observe(widget);
  }

  void Wait() { run_loop_.Run(); }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override {
    if (bounds.size() == expected_size_) {
      run_loop_.Quit();
    }
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  gfx::Size expected_size_;
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
};

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       InnerBoundsMatchRequest) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());

  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto kInitialPipSize = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, kInitialPipSize);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);

  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  auto* pip_browser_view = BrowserView::GetBrowserViewForBrowser(pip_browser);
  EXPECT_EQ(kInitialPipSize, pip_browser_view->GetContentsSize());
}

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       AppWindowWebContentsSizeUnchangedAfterExitPip) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());
  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

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

  // Exit Picture-in-Picture.
  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  pip_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(pip_browser);
  EXPECT_FALSE(picture_in_picture_mixin_test_base_.window_controller()
                   ->GetChildWebContents());

  // Verify that the app browser WebContents size has not changed.
  EXPECT_EQ(expected_app_browser_web_contents_size,
            app_browser_view->GetContentsSize());
}

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       ResizeToRespectsMinimumInnerWindowSize) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());
  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto kInitialPipSize = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, kInitialPipSize);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);

  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  auto* pip_browser_view = BrowserView::GetBrowserViewForBrowser(pip_browser);
  EXPECT_EQ(kInitialPipSize, pip_browser_view->GetContentsSize());

  // Resize Pip window to a size smaller than the allowed minimum.
  EXPECT_TRUE(ExecJs(pip_web_contents, "window.resizeTo(50,50);"));

  // TODO(crbug.com/354785208): Replace with `WidgetResizeWaiter` once bug is
  // fixed.
  base::RunLoop().RunUntilIdle();

  // Verify that the minimum inner window size is respected.
  EXPECT_GE(pip_browser_view->GetContentsSize().width(),
            PictureInPictureWindowManager::GetMinimumInnerWindowSize().width());
  EXPECT_GE(
      pip_browser_view->GetContentsSize().height(),
      PictureInPictureWindowManager::GetMinimumInnerWindowSize().height());

  // Resize Pip window, one more time, to a size smaller than the allowed
  // minimum.
  EXPECT_TRUE(ExecJs(pip_web_contents, "window.resizeTo(50,50);"));

  // TODO(crbug.com/354785208): Replace with `WidgetResizeWaiter` once bug is
  // fixed.
  base::RunLoop().RunUntilIdle();

  // Verify that the minimum inner window size is respected.
  EXPECT_GE(pip_browser_view->GetContentsSize().width(),
            PictureInPictureWindowManager::GetMinimumInnerWindowSize().width());
  EXPECT_GE(
      pip_browser_view->GetContentsSize().height(),
      PictureInPictureWindowManager::GetMinimumInnerWindowSize().height());
}

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       ResizeToRespectsMaximumWindowSize) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());

  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto kInitialPipSize = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, kInitialPipSize);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);

  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  auto* pip_browser_view = BrowserView::GetBrowserViewForBrowser(pip_browser);
  EXPECT_EQ(kInitialPipSize, pip_browser_view->GetContentsSize());

  const BrowserWindow* const pip_browser_window = pip_browser->window();
  const gfx::NativeWindow native_window = pip_browser_window->GetNativeWindow();
  const display::Screen* const screen = display::Screen::GetScreen();
  const display::Display display =
      screen->GetDisplayNearestWindow(native_window);

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
        pip_browser_view->GetWidget(),
        PictureInPictureWindowManager::GetMaximumWindowSize(display));
    EXPECT_TRUE(ExecJs(pip_web_contents, script));
    waiter.Wait();
  }

  EXPECT_EQ(pip_browser_view->GetBounds().size(), maximum_window_size);
}

}  // namespace
