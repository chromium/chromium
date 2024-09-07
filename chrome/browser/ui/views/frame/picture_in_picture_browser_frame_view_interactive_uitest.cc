// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/widget/widget_utils.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/linux/fake_linux_ui.h"
#include "ui/linux/linux_ui_getter.h"
#endif

namespace {

using ::testing::WithParamInterface;

// Contains all expectations associated with animations at a given point in
// time.
struct ExpectationsAtTimeDelta {
  base::TimeDelta time_delta;

  // Whether the back to tab button is expected to be visible, or not. Optional
  // since the associated button view is null when `disallow_return_to_opener`
  // is true.
  std::optional<bool> expected_back_to_tab_button_is_visible;

  // Whether the close button is expected to be visible, or not.
  bool expected_close_button_is_visible;

  // Whether any of the content setting view/s is/are expected to be visible or
  // not.
  bool expected_has_any_visible_content_setting_views;
};

struct AnimationTimingTestCase {
  std::string test_name;
  bool has_content_settings_view;
  bool disallow_return_to_opener;
  std::vector<ExpectationsAtTimeDelta> show_expectations;
  std::vector<ExpectationsAtTimeDelta> hide_expectations;
};

using AnimationTimingTest = WithParamInterface<AnimationTimingTestCase>;

constexpr base::TimeDelta kFirstAnimationInterval = base::Milliseconds(25);
constexpr base::TimeDelta kSecondAnimationInterval = base::Milliseconds(75);
constexpr base::TimeDelta kThirdAnimationInterval = base::Milliseconds(125);
constexpr base::TimeDelta kFourthAnimationInterval = base::Milliseconds(175);
constexpr base::TimeDelta kFifthAnimationInterval = base::Milliseconds(225);
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

const base::FilePath::CharType kPictureInPictureDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");
const base::FilePath::CharType kCameraPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-camera.html");

class AnimationWaiter {
 public:
  explicit AnimationWaiter(std::vector<gfx::Animation*> animations)
      : animations_(animations) {}

  AnimationWaiter() = delete;
  AnimationWaiter(const AnimationWaiter&) = delete;
  AnimationWaiter(AnimationWaiter&&) = delete;
  AnimationWaiter& operator=(const AnimationWaiter&) = delete;

  void WaitForAnimationInterval(base::TimeDelta animation_interval) {
    for (auto* animation : animations_) {
      auto animation_api = std::make_unique<gfx::AnimationTestApi>(animation);
      animation_api->SetStartTime(waiter_creation_time_);
      animation_api->Step(waiter_creation_time_ + animation_interval);
    }
  }

 private:
  const base::TimeTicks waiter_creation_time_ = base::TimeTicks::Now();
  std::vector<gfx::Animation*> animations_;
};

class PictureInPictureBrowserFrameViewTest : public WebRtcTestBase,
                                             public AnimationTimingTest {
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

  void SetUpDocumentPIP(
      std::optional<bool> disallow_return_to_opener = std::nullopt,
      const base::FilePath::CharType* pip_page_relative_path =
          kPictureInPictureDocumentPipPage) {
    // Navigate to test url.
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(pip_page_relative_path));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

    content::WebContents* active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    // Enter document pip.
    auto* pip_window_controller_ = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(active_web_contents);
    std::string disallow_return_to_opener_js_string =
        (disallow_return_to_opener.has_value()
             ? (*disallow_return_to_opener ? "true" : "false")
             : "undefined");

    if (pip_page_relative_path == kCameraPage) {
      GetUserMediaAndAccept(active_web_contents);

      // Open a picture-in-picture window manually.
      content::MediaStartStopObserver enter_pip_observer(
          active_web_contents,
          content::MediaStartStopObserver::Type::kEnterPictureInPicture);
      active_web_contents->GetPrimaryMainFrame()
          ->ExecuteJavaScriptWithUserGestureForTests(
              base::StrCat(
                  {u"openPip({disallowReturnToOpener: ",
                   base::UTF8ToUTF16(disallow_return_to_opener_js_string),
                   u"})"}),
              base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
      enter_pip_observer.Wait();
    } else {
      ASSERT_EQ(true,
                EvalJs(active_web_contents,
                       base::StrCat(
                           {"createDocumentPipWindow({disallowReturnToOpener: ",
                            disallow_return_to_opener_js_string, "})"})));
    }

    // A pip window should have opened.
    EXPECT_TRUE(active_web_contents->HasPictureInPictureDocument());
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

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(pip_frame_view_->GetWidget()));
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
      is_button_visible &= (button_view->layer()->opacity() > 0.0f);
    }
    return is_button_visible;
  }

  bool IsPointInPIPFrameView(gfx::Point point_in_screen) {
    views::View::ConvertPointFromScreen(pip_frame_view_, &point_in_screen);
    return pip_frame_view_->GetLocalBounds().Contains(point_in_screen);
  }

  void UpdateTopBarView(const gfx::Point& point) {
// The platform specific code is needed because Mac does not detect mouse events
// that are sent outside the pip window.
#if BUILDFLAG(IS_MAC)
    pip_frame_view()->UpdateTopBarView(IsPointInPIPFrameView(point));
#else
    event_generator_->MoveMouseTo(point);
#endif
  }

#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
  std::unique_ptr<views::Widget> OpenChildDialog(const gfx::Size& size) {
    views::Widget::InitParams init_params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
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
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
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

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       WindowTitleUsesOpenersTitle) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  // The window title for the document picture-in-picture window should use the
  // title from the opener page.
  EXPECT_EQ(
      u"Document Picture-in-Picture",
      pip_frame_view()->browser_view()->browser()->GetWindowTitleForCurrentTab(
          /*include_app_name=*/false));
}

#if BUILDFLAG(IS_LINUX)

class FakeLinuxUiGetter : public ui::LinuxUiGetter {
 public:
  FakeLinuxUiGetter() = default;

  ui::LinuxUiTheme* GetForWindow(aura::Window* window) override {
    return &fake_linux_ui_;
  }

  ui::LinuxUiTheme* GetForProfile(Profile* profile) override {
    return &fake_linux_ui_;
  }

 private:
  class LinuxUiWithoutNativeDecoration : public ui::FakeLinuxUi {
   public:
    ui::NativeTheme* GetNativeTheme() const override {
      return ui::NativeTheme::GetInstanceForNativeUi();
    }

    ui::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame,
                                                    bool tiled) override {
      // The test relies on this returning null.
      return nullptr;
    }
  };

  LinuxUiWithoutNativeDecoration fake_linux_ui_;
};

class PictureInPictureBrowserFrameViewLinuxNoClientNativeDecorationsTest
    : public PictureInPictureBrowserFrameViewTest {
 public:
  void SetUpOnMainThread() override {
    // Create a fake UI getter, which will automatically set itself as the
    // default. This has to wait until `SetUpOnMainThread()` so browser startup
    // doesn't overwrite it with the real getter.
    linux_ui_getter_ = std::make_unique<FakeLinuxUiGetter>();
    ThemeServiceFactory::GetForProfile(browser()->profile())->UseSystemTheme();
    PictureInPictureBrowserFrameViewTest::SetUpOnMainThread();
  }

 private:
  std::unique_ptr<ui::LinuxUiGetter> linux_ui_getter_;
};

// Regression test for https://crbug.com/325459394:
// PiP should not crash if the Linux native theme does not draw client-side
// frame decorations.
IN_PROC_BROWSER_TEST_F(
    PictureInPictureBrowserFrameViewLinuxNoClientNativeDecorationsTest,
    DoesNotCrash) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());
}

#endif

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       RespectsDisallowReturnToOpenerWhenDefault) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP());

  // The back-to-tab button should exist when `disallowReturnToOpener` is not
  // specified.
  EXPECT_NE(nullptr, pip_frame_view()->GetBackToTabButtonForTesting());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       RespectsDisallowReturnToOpenerWhenTrue) {
  ASSERT_NO_FATAL_FAILURE(SetUpDocumentPIP(/*disallow_return_to_opener=*/true));

  // The back-to-tab button should not exist when `disallowReturnToOpener` is
  // true.
  EXPECT_EQ(nullptr, pip_frame_view()->GetBackToTabButtonForTesting());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserFrameViewTest,
                       RespectsDisallowReturnToOpenerWhenFalse) {
  ASSERT_NO_FATAL_FAILURE(
      SetUpDocumentPIP(/*disallow_return_to_opener=*/false));

  // The back-to-tab button should exist when `disallowReturnToOpener` is false.
  EXPECT_NE(nullptr, pip_frame_view()->GetBackToTabButtonForTesting());
}

IN_PROC_BROWSER_TEST_P(PictureInPictureBrowserFrameViewTest,
                       TestAnimationTiming) {
  const AnimationTimingTestCase& test_case = GetParam();
  test_case.has_content_settings_view
      ? SetUpDocumentPIP(
            /*disallow_return_to_opener=*/test_case.disallow_return_to_opener,
            kCameraPage)
      : SetUpDocumentPIP(
            /*disallow_return_to_opener=*/test_case.disallow_return_to_opener);

  // Move mouse to the center of the pip window should activate title.
  gfx::Point center = pip_frame_view()->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToScreen(pip_frame_view(), &center);
  ASSERT_TRUE(IsPointInPIPFrameView(center));
  UpdateTopBarView(center);

  AnimationWaiter show_animation_waiter(
      pip_frame_view()->GetRenderActiveAnimationsForTesting());
  for (size_t i = 0; i < test_case.show_expectations.size(); ++i) {
    const auto& show_expectations = test_case.show_expectations[i];

    SCOPED_TRACE(
        base::StringPrintf("Show expectation # %zu, time delta used: %0.1f ms",
                           i, show_expectations.time_delta.InMillisecondsF()));

    show_animation_waiter.WaitForAnimationInterval(
        show_expectations.time_delta);
    if (test_case.disallow_return_to_opener) {
      DCHECK(!show_expectations.expected_back_to_tab_button_is_visible);
      ASSERT_EQ(nullptr, pip_frame_view()->GetBackToTabButtonForTesting());
    } else {
      ASSERT_EQ(
          show_expectations.expected_back_to_tab_button_is_visible,
          IsButtonVisible(pip_frame_view()->GetBackToTabButtonForTesting()));
    }
    ASSERT_EQ(show_expectations.expected_close_button_is_visible,
              IsButtonVisible(pip_frame_view()->GetCloseButtonForTesting()));
    ASSERT_EQ(show_expectations.expected_has_any_visible_content_setting_views,
              pip_frame_view()->HasAnyVisibleContentSettingViews());
  }

  // Move mouse to the top-left corner of the main browser window (out side of
  // the pip window) should deactivate the title.
  gfx::Point outside = gfx::Point();
  views::View::ConvertPointToScreen(
      static_cast<BrowserView*>(browser()->window()), &outside);
  ASSERT_FALSE(IsPointInPIPFrameView(outside));
  UpdateTopBarView(outside);

  AnimationWaiter hide_animation_waiter(
      pip_frame_view()->GetRenderInactiveAnimationsForTesting());
  for (size_t i = 0; i < test_case.hide_expectations.size(); ++i) {
    const auto& hide_expectations = test_case.hide_expectations[i];

    SCOPED_TRACE(
        base::StringPrintf("Hide expectation # %zu, time delta used: %0.1f ms",
                           i, hide_expectations.time_delta.InMillisecondsF()));

    hide_animation_waiter.WaitForAnimationInterval(
        hide_expectations.time_delta);
    if (test_case.disallow_return_to_opener) {
      DCHECK(!hide_expectations.expected_back_to_tab_button_is_visible);
      ASSERT_EQ(nullptr, pip_frame_view()->GetBackToTabButtonForTesting());
    } else {
      ASSERT_EQ(
          hide_expectations.expected_back_to_tab_button_is_visible,
          IsButtonVisible(pip_frame_view()->GetBackToTabButtonForTesting()));
    }
    ASSERT_EQ(hide_expectations.expected_close_button_is_visible,
              IsButtonVisible(pip_frame_view()->GetCloseButtonForTesting()));
    ASSERT_EQ(hide_expectations.expected_has_any_visible_content_setting_views,
              pip_frame_view()->HasAnyVisibleContentSettingViews());
  }
}

INSTANTIATE_TEST_SUITE_P(
    AnimationTimingTestSuiteInstantiation,
    PictureInPictureBrowserFrameViewTest,
    testing::ValuesIn<AnimationTimingTestCase>({
        {.test_name = "WithoutContentSettingView_WithBackToTabButton",
         .has_content_settings_view = false,
         .disallow_return_to_opener = false,
         .show_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false}},
         .hide_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = false,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = false}}},
        {.test_name = "WithoutContentSettingView_WithoutBackToTabButton",
         .has_content_settings_view = false,
         .disallow_return_to_opener = true,
         .show_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false}},
         .hide_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = false},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = false}}},
        {.test_name = "WithContentSettingView_WithBackToTabButton",
         .has_content_settings_view = true,
         .disallow_return_to_opener = false,
         .show_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true}},
         .hide_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = true,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = false,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = false,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = false,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = false,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = true}}},
        {.test_name = "WithContentSettingView_WithoutBackToTabButton",
         .has_content_settings_view = true,
         .disallow_return_to_opener = true,
         .show_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true}},
         .hide_expectations =
             {ExpectationsAtTimeDelta{
                  .time_delta = kFirstAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kSecondAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kThirdAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = true,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFourthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kFifthAnimationInterval,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = true},
              ExpectationsAtTimeDelta{
                  .time_delta = kAnimationDuration,
                  .expected_back_to_tab_button_is_visible = std::nullopt,
                  .expected_close_button_is_visible = false,
                  .expected_has_any_visible_content_setting_views = true}}},
    }),
    [](const testing::TestParamInfo<AnimationTimingTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
