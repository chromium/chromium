// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

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
  web_app::WebAppPictureInPictureMixinTestBase
      picture_in_picture_mixin_test_base_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       InnerBoundsMatchRequest) {
  const webapps::AppId app_id =
      InstallPWA(picture_in_picture_mixin_test_base_.GetPictureInPictureURL());

  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto size = gfx::Size(400, 450);
  picture_in_picture_mixin_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser, size);

  auto* pip_web_contents =
      picture_in_picture_mixin_test_base_.window_controller()
          ->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_mixin_test_base_.WaitForPageLoad(pip_web_contents);

  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(pip_browser);
  EXPECT_EQ(size, browser_view->GetContentsSize());
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

}  // namespace
