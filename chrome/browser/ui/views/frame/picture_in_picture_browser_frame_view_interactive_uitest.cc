// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

namespace {

const base::FilePath::CharType kPictureInPictureDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

class PictureInPictureBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  PictureInPictureBrowserFrameViewTest() = default;

  PictureInPictureBrowserFrameViewTest(
      const PictureInPictureBrowserFrameViewTest&) = delete;
  PictureInPictureBrowserFrameViewTest& operator=(
      const PictureInPictureBrowserFrameViewTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("8", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kDocumentPictureInPictureAPI);
    InProcessBrowserTest::SetUp();
  }

  void SetUpDocumentPIP() {
    // Navigate to test url.
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kPictureInPictureDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

    content::WebContents* active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    // Enter document pip.
    auto* pip_window_controller_ = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(active_web_contents);
    ASSERT_EQ(true, EvalJs(active_web_contents, "createDocumentPipWindow()"));
    ASSERT_NE(nullptr, pip_window_controller_);

    auto* child_web_contents = pip_window_controller_->GetChildWebContents();
    ASSERT_TRUE(child_web_contents);

    auto* browser_view = static_cast<BrowserView*>(
        BrowserWindow::FindBrowserWindowWithWebContents(child_web_contents));
    ASSERT_TRUE(browser_view);
    ASSERT_TRUE(browser_view->browser()->is_type_picture_in_picture());

    pip_frame_view_ = static_cast<PictureInPictureBrowserFrameView*>(
        browser_view->frame()->GetFrameView());
    ASSERT_TRUE(pip_frame_view_);
  }

  bool IsPointInPIPFrameView(gfx::Point point_in_screen) {
    views::View::ConvertPointFromScreen(pip_frame_view_, &point_in_screen);
    return pip_frame_view_->GetLocalBounds().Contains(point_in_screen);
  }

  PictureInPictureBrowserFrameView* pip_frame_view() { return pip_frame_view_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  PictureInPictureBrowserFrameView* pip_frame_view_ = nullptr;
};

#if BUILDFLAG(IS_WIN)
// Document PIP is not supported in LACROS.
// TODO(jazzhsu): Fix test on MAC and Wayland. Test currently not working on
// those platforms because if we send mouse move event outside of the pip window
// in ui_test_utils::SendMouseMoveSync, the pip window will not receive the
// event.
#define MAYBE_TitleActivation TitleActivation
#else
#define MAYBE_TitleActivation DISABLED_TitleActivation
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       MAYBE_TitleActivation) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  // Move mouse to the center of the pip window should activate title.
  gfx::Point center = pip_frame_view()->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToScreen(pip_frame_view(), &center);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(center));
  ASSERT_TRUE(pip_frame_view()->GetBackToTabButtonForTesting()->GetVisible());

  // Move mouse to the top-left corner of the main browser window (out side of
  // the pip window) should deactivate the title.
  gfx::Point outside = gfx::Point();
  views::View::ConvertPointToScreen(
      static_cast<BrowserView*>(browser()->window()), &outside);
  ASSERT_FALSE(IsPointInPIPFrameView(outside));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(outside));
  ASSERT_FALSE(pip_frame_view()->GetBackToTabButtonForTesting()->GetVisible());

  // Move mouse back in pip window should activate title.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(center));
  ASSERT_TRUE(pip_frame_view()->GetBackToTabButtonForTesting()->GetVisible());
}

}  // namespace
