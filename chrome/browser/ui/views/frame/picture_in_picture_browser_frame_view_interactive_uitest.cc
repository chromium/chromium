// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace {

constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kDocumentPictureInPictureAPI,
                              media::kPictureInPictureOcclusionTracking},
        /*disabled_features=*/{});
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

  void WaitForTopBarAnimations(std::vector<gfx::Animation*> animations) {
    base::TimeTicks now = base::TimeTicks::Now();
    for (auto* animation : animations) {
      gfx::AnimationTestApi animation_api(animation);
      animation_api.SetStartTime(now);
      animation_api.Step(now + kAnimationDuration);
    }
  }

  bool IsButtonVisible(views::View* button_view) {
    bool is_button_visible = button_view->GetVisible();
    if (button_view->layer() != nullptr) {
      is_button_visible &= (button_view->layer()->opacity() > 0);
    }
    return is_button_visible;
  }

  bool IsPointInPIPFrameView(gfx::Point point_in_screen) {
    views::View::ConvertPointFromScreen(pip_frame_view_, &point_in_screen);
    return pip_frame_view_->GetLocalBounds().Contains(point_in_screen);
  }

#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
  std::unique_ptr<views::Widget> OpenChildDialog(const gfx::Size& size) {
    views::Widget::InitParams init_params(
        views::Widget::InitParams::TYPE_WINDOW);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.child = true;
    init_params.parent = pip_frame_view_->GetWidget()->GetNativeWindow();

    auto child_dialog = std::make_unique<views::Widget>(std::move(init_params));
    child_dialog->GetContentsView()->SetPreferredSize(size);
    child_dialog->SetSize(size);
    child_dialog->Show();
    return child_dialog;
  }
#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG

  PictureInPictureBrowserFrameView* pip_frame_view() { return pip_frame_view_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<PictureInPictureBrowserFrameView, AcrossTasksDanglingUntriaged>
      pip_frame_view_ = nullptr;
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
  WaitForTopBarAnimations(
      pip_frame_view()->GetRenderActiveAnimationsForTesting());
  ASSERT_TRUE(
      IsButtonVisible(pip_frame_view()->GetBackToTabButtonForTesting()));
  ASSERT_TRUE(IsButtonVisible(pip_frame_view()->GetCloseButtonForTesting()));

  // Move mouse to the top-left corner of the main browser window (out side of
  // the pip window) should deactivate the title.
  gfx::Point outside = gfx::Point();
  views::View::ConvertPointToScreen(
      static_cast<BrowserView*>(browser()->window()), &outside);
  ASSERT_FALSE(IsPointInPIPFrameView(outside));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(outside));
  WaitForTopBarAnimations(
      pip_frame_view()->GetRenderInactiveAnimationsForTesting());
  ASSERT_FALSE(
      IsButtonVisible(pip_frame_view()->GetBackToTabButtonForTesting()));
  ASSERT_FALSE(IsButtonVisible(pip_frame_view()->GetCloseButtonForTesting()));

  // Move mouse back in pip window should activate title.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(center));
  WaitForTopBarAnimations(
      pip_frame_view()->GetRenderActiveAnimationsForTesting());
  ASSERT_TRUE(
      IsButtonVisible(pip_frame_view()->GetBackToTabButtonForTesting()));
  ASSERT_TRUE(IsButtonVisible(pip_frame_view()->GetCloseButtonForTesting()));
}

#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       ResizesToFitChildDialogs) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  gfx::Rect initial_pip_bounds =
      pip_frame_view()->GetWidget()->GetWindowBoundsInScreen();

  // Open a child dialog that is larger than the pip window.
  const gfx::Size child_dialog_size(initial_pip_bounds.width() + 20,
                                    initial_pip_bounds.height() + 10);
  auto child_dialog = OpenChildDialog(child_dialog_size);

  // The pip window should increase its size to contain the child dialog.
  gfx::Rect new_pip_bounds =
      pip_frame_view()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_NE(initial_pip_bounds, new_pip_bounds);
  EXPECT_GE(new_pip_bounds.width(), child_dialog_size.width());
  EXPECT_GE(new_pip_bounds.height(), child_dialog_size.height());

  // Close the dialog.
  child_dialog->CloseNow();

  // The pip window should return to its original bounds.
  EXPECT_EQ(initial_pip_bounds,
            pip_frame_view()->GetWidget()->GetWindowBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       RespectsUserLocationChangesAfterChildDialogCloses) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  gfx::Rect initial_pip_bounds =
      pip_frame_view()->GetWidget()->GetWindowBoundsInScreen();

  // Open a child dialog that is larger than the pip window.
  const gfx::Size child_dialog_size(initial_pip_bounds.width() + 20,
                                    initial_pip_bounds.height() + 10);
  auto child_dialog = OpenChildDialog(child_dialog_size);

  // The pip window should increase its size to contain the child dialog.
  gfx::Rect new_pip_bounds =
      pip_frame_view()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_NE(initial_pip_bounds, new_pip_bounds);
  EXPECT_GE(new_pip_bounds.width(), child_dialog_size.width());
  EXPECT_GE(new_pip_bounds.height(), child_dialog_size.height());

  // The user then moves the dialog.
  gfx::Rect moved_bounds = new_pip_bounds;
  moved_bounds.set_x(moved_bounds.x() - 10);
  moved_bounds.set_y(moved_bounds.y() - 10);
  pip_frame_view()->GetWidget()->SetBounds(moved_bounds);

  // Close the dialog.
  child_dialog->CloseNow();

  // Since the user moved the window but did not resize, it should return to
  // its original size but keep the new position.
  gfx::Rect expected_final_bounds = moved_bounds;
  expected_final_bounds.set_width(initial_pip_bounds.width());
  expected_final_bounds.set_height(initial_pip_bounds.height());
  EXPECT_EQ(expected_final_bounds,
            pip_frame_view()->GetWidget()->GetWindowBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       RespectsUserBoundsChangesAfterChildDialogCloses) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  gfx::Rect initial_pip_bounds =
      pip_frame_view()->GetWidget()->GetWindowBoundsInScreen();

  // Open a child dialog that is larger than the pip window.
  const gfx::Size child_dialog_size(initial_pip_bounds.width() + 20,
                                    initial_pip_bounds.height() + 10);
  auto child_dialog = OpenChildDialog(child_dialog_size);

  // The pip window should increase its size to contain the child dialog.
  gfx::Rect new_pip_bounds =
      pip_frame_view()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_NE(initial_pip_bounds, new_pip_bounds);
  EXPECT_GE(new_pip_bounds.width(), child_dialog_size.width());
  EXPECT_GE(new_pip_bounds.height(), child_dialog_size.height());

  // The user then moves and resizes the dialog.
  gfx::Rect moved_bounds = new_pip_bounds;
  moved_bounds.set_width(moved_bounds.width() + 10);
  moved_bounds.set_height(moved_bounds.height() + 10);
  moved_bounds.set_x(moved_bounds.x() - 10);
  moved_bounds.set_y(moved_bounds.y() - 10);
  pip_frame_view()->GetWidget()->SetBounds(moved_bounds);

  // Close the dialog.
  child_dialog->CloseNow();

  // Since the user both moved and resized the window, it should not change back
  // when the child dialog closes.
  EXPECT_EQ(moved_bounds,
            pip_frame_view()->GetWidget()->GetWindowBoundsInScreen());
}

#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       TitleActivatesWithOverlayView) {
  // Verify that the title bar is on when the overlay view is shown.

  // Pretend that we're in auto-pip so that we get an overlay view.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Ensure that there is a helper for `web_contents`.  This will no-op if
  // something has already created it, but right now it's dependent on having
  // the feature enabled.
  AutoPictureInPictureTabHelper::CreateForWebContents(web_contents);
  auto* auto_pip_tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  auto_pip_tab_helper->set_is_in_auto_picture_in_picture_for_testing(true);
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  // The title buttons should be visible.
  WaitForTopBarAnimations(
      pip_frame_view()->GetRenderActiveAnimationsForTesting());
  ASSERT_TRUE(
      IsButtonVisible(pip_frame_view()->GetBackToTabButtonForTesting()));
  ASSERT_TRUE(IsButtonVisible(pip_frame_view()->GetCloseButtonForTesting()));
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       IsTrackedByTheOcclusionObserver) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  PictureInPictureOcclusionTracker* occlusion_tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  ASSERT_TRUE(occlusion_tracker);

  {
    std::vector<views::Widget*> pip_widgets =
        occlusion_tracker->GetPictureInPictureWidgetsForTesting();

    // Check that the PictureInPictureOcclusionTracker is observing the
    // document picture-in-picture window.
    EXPECT_EQ(1u, pip_widgets.size());
    EXPECT_EQ(pip_frame_view()->GetWidget(), pip_widgets[0]);
  }

  // Open the PageInfo dialog and ensure that it's being tracked as well. We
  // don't have a handle to the widget, but we can reasonably assume it's being
  // tracked if the number of tracked widgets is now 2.
  {
    pip_frame_view()->ShowPageInfoDialog();
    std::vector<views::Widget*> pip_widgets =
        occlusion_tracker->GetPictureInPictureWidgetsForTesting();
    EXPECT_EQ(2u, pip_widgets.size());
  }

  // Close both widgets and ensure they're no longer being tracked.
  {
    pip_frame_view()->GetWidget()->CloseNow();
    std::vector<views::Widget*> pip_widgets =
        occlusion_tracker->GetPictureInPictureWidgetsForTesting();
    EXPECT_EQ(0u, pip_widgets.size());
  }
}

}  // namespace
