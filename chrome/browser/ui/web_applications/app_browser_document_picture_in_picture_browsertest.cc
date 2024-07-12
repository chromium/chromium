// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using content::EvalJs;

namespace {

const base::FilePath::CharType kPictureInPictureDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

class AppBrowserDocumentPictureInPictureBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  AppBrowserDocumentPictureInPictureBrowserTest() = default;
  AppBrowserDocumentPictureInPictureBrowserTest(
      const AppBrowserDocumentPictureInPictureBrowserTest&) = delete;
  AppBrowserDocumentPictureInPictureBrowserTest& operator=(
      const AppBrowserDocumentPictureInPictureBrowserTest&) = delete;
  ~AppBrowserDocumentPictureInPictureBrowserTest() override = default;

  void PostRunTestOnMainThread() override {
    // To avoid having a dangling raw_ptr, destroy the `pip_window_controller_`
    // before the test class.
    pip_window_controller_ = nullptr;
    InProcessBrowserTest::PostRunTestOnMainThread();
  }

  GURL GetPictureInPictureURL() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kPictureInPictureDocumentPipPage));
  }

  void NavigateToURLAndEnterPictureInPicture(
      Browser* browser,
      const gfx::Size& window_size = gfx::Size(500, 500)) {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser, GetPictureInPictureURL()));

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    const std::string script = base::StrCat(
        {"createDocumentPipWindow({width:",
         base::NumberToString(window_size.width()),
         ",height:", base::NumberToString(window_size.height()), "})"});
    ASSERT_EQ(true, EvalJs(active_web_contents, script));
    ASSERT_TRUE(window_controller() != nullptr);
    // Especially on Linux, this isn't synchronous.
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&content::RenderWidgetHostView::IsShowing,
                            base::Unretained(GetRenderWidgetHostView())),
        true, base::Seconds(30))
        .Wait();
    ASSERT_TRUE(GetRenderWidgetHostView()->IsShowing());
  }

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    if (!window_controller()) {
      return nullptr;
    }

    if (auto* web_contents = window_controller()->GetChildWebContents()) {
      return web_contents->GetRenderWidgetHostView();
    }

    return nullptr;
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(web_contents);
  }

  content::DocumentPictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

  void WaitForPageLoad(content::WebContents* contents) {
    EXPECT_TRUE(WaitForLoadStop(contents));
    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
  }

 private:
  raw_ptr<content::DocumentPictureInPictureWindowController>
      pip_window_controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       InnerBoundsMatchRequest) {
  const webapps::AppId app_id = InstallPWA(GetPictureInPictureURL());

  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  constexpr auto size = gfx::Size(400, 450);
  NavigateToURLAndEnterPictureInPicture(browser, size);

  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(pip_browser);
  EXPECT_EQ(size, browser_view->GetContentsSize());
}

IN_PROC_BROWSER_TEST_F(AppBrowserDocumentPictureInPictureBrowserTest,
                       AppWindowWebContentsSizeUnchangedAfterExitPip) {
  const webapps::AppId app_id = InstallPWA(GetPictureInPictureURL());
  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  // Navigate to the Picture-in-Picture URL, enter Picture-in-Picture and
  // remember the app browser WebContents size.
  NavigateToURLAndEnterPictureInPicture(browser);
  auto* app_browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  const auto expected_app_browser_web_contents_size =
      app_browser_view->GetContentsSize();

  // Verify that we have entered Picture-in-Picture.
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  // Exit Picture-in-Picture.
  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  pip_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(pip_browser);
  EXPECT_FALSE(window_controller()->GetChildWebContents());

  // Verify that the app browser WebContents size has not changed.
  EXPECT_EQ(expected_app_browser_web_contents_size,
            app_browser_view->GetContentsSize());
}

}  // namespace
