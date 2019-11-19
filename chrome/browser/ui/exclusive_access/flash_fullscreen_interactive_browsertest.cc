// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ppapi/ppapi_test.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

#if defined(OS_MACOSX)
const bool kIsMacUI = true;
#else
const bool kIsMacUI = false;
#endif

// Runs the current MessageLoop until |condition| is true or timeout.
bool RunLoopUntil(const base::Callback<bool()>& condition) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (!condition.Run()) {
    const base::TimeTicks current_time = base::TimeTicks::Now();
    if (current_time - start_time > base::TimeDelta::FromSeconds(10)) {
      ADD_FAILURE() << "Condition not met within ten seconds.";
      return false;
    }

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
        base::TimeDelta::FromMilliseconds(20));
    content::RunMessageLoop();
  }
  return true;
}

}  // namespace

// A BrowserTest that opens a test page that launches a simulated fullscreen
// Flash plugin.  The plugin responds to mouse clicks and key presses by
// changing color.  Once launched, the browser UI can be tested to confirm the
// desired interactive behaviors.
class FlashFullscreenInteractiveBrowserTest : public OutOfProcessPPAPITest {
 public:
  FlashFullscreenInteractiveBrowserTest() {}
  ~FlashFullscreenInteractiveBrowserTest() override {}

 protected:
  content::WebContents* GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // A simple way to convince libcontent and the browser UI that a tab is being
  // screen captured.  During tab capture, Flash fullscreen remains embedded
  // within the tab content area of a non-fullscreened browser window.
  void StartFakingTabCapture() {
    GetActiveWebContents()->IncrementCapturerCount(gfx::Size(360, 240),
                                                   /* stay_hidden */ false);
  }

  bool LaunchFlashFullscreen() {
    // This navigates to a page that runs the simulated fullscreen Flash
    // plugin.  It will block until the plugin has completed an attempt to enter
    // Flash fullscreen mode.
    OutOfProcessPPAPITest::RunTest("FlashFullscreenForBrowserUI");

    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << ("Failed to launch simulated fullscreen Flash plugin.  "
                        "Interactive UI testing cannot proceed.");
      return false;
    }

    EXPECT_TRUE(ObserveTabIsInFullscreen(true));

    return !::testing::Test::HasFailure();
  }

  bool LaunchFlashFullscreenInSubframe() {
    // Start the embedded test server and set it up to serve PPAPI test case
    // URLs.
    base::FilePath document_root;
    EXPECT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&document_root));
    embedded_test_server()->AddDefaultHandlers(document_root);
    if (!embedded_test_server()->Start()) {
      ADD_FAILURE() << "Failed to launch embedded test server.";
      return false;
    }

    // Load a page with an <iframe> that points to the test case URL, which
    // runs the simulated fullscreen Flash plugin.  In OOPIF modes, the frame
    // will render in a separate process.  Block until the plugin has completed
    // an attempt to enter Flash fullscreen mode.
    GURL test_url = GetTestURL(*embedded_test_server(),
                               "FlashFullscreenForBrowserUI", std::string());
    GURL main_url("data:text/html,<iframe src='" + test_url.spec() +
                  "'></iframe>");
    OutOfProcessPPAPITest::RunTestURL(main_url);

    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << ("Failed to launch simulated fullscreen Flash plugin.  "
                        "Interactive UI testing cannot proceed.");
      return false;
    }

    EXPECT_TRUE(ObserveTabIsInFullscreen(true));

    return !::testing::Test::HasFailure();
  }

  void UseAcceleratorToOpenNewTab() {
    content::WebContents* const old_tab_contents = GetActiveWebContents();
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_T, !kIsMacUI, false, false, kIsMacUI));
    EXPECT_TRUE(RunLoopUntil(base::Bind(
        &FlashFullscreenInteractiveBrowserTest::IsObservingActiveWebContents,
        base::Unretained(this),
        old_tab_contents,
        false)));
  }

  void UseAcceleratorToSwitchToTab(int tab_index) {
    content::WebContents* const old_tab_contents = GetActiveWebContents();
    const ui::KeyboardCode key_code =
        static_cast<ui::KeyboardCode>(ui::VKEY_1 + tab_index);
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), key_code, !kIsMacUI, false, false, kIsMacUI));
    EXPECT_TRUE(RunLoopUntil(base::Bind(
        &FlashFullscreenInteractiveBrowserTest::IsObservingActiveWebContents,
        base::Unretained(this),
        old_tab_contents,
        false)));
  }

  void PressEscape() {
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_ESCAPE, false, false, false, false));
  }

  void PressSpacebar() {
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_SPACE, false, false, false, false));
  }

  void SpamSpacebar() {
    for (int i = 0; i < 11; ++i)
      PressSpacebar();
  }

  void ClickOnTabContainer() {
    ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  }

  void ClickOnOmnibox() {
    ui_test_utils::ClickOnView(browser(), VIEW_ID_OMNIBOX);
  }

  bool ObserveTabIsInFullscreen(bool expected_in_fullscreen) const {
    if (!RunLoopUntil(base::Bind(
            &FlashFullscreenInteractiveBrowserTest::IsObservingTabInFullscreen,
            base::Unretained(this),
            GetActiveWebContents(),
            expected_in_fullscreen)))
      return false;

    if (expected_in_fullscreen) {
      if (!GetActiveWebContents()->GetFullscreenRenderWidgetHostView()) {
        ADD_FAILURE()
            << "WebContents should have a fullscreen RenderWidgetHostView.";
        return false;
      }
      EXPECT_EQ(GetActiveWebContents()->IsBeingCaptured(),
                !browser()
                     ->exclusive_access_manager()
                     ->fullscreen_controller()
                     ->IsWindowFullscreenForTabOrPending());
    }

    return true;
  }

  bool ObserveFlashHasFocus(content::WebContents* contents,
                            bool expected_to_have_focus) const {
    if (!RunLoopUntil(base::Bind(
            &FlashFullscreenInteractiveBrowserTest::IsObservingFlashHasFocus,
            base::Unretained(this),
            contents,
            expected_to_have_focus)))
      return false;

    if (expected_to_have_focus) {
      content::RenderWidgetHostView* const web_page_view =
          contents->GetRenderWidgetHostView();
      EXPECT_FALSE(web_page_view && web_page_view->HasFocus())
          << "Both RenderWidgetHostViews cannot have focus at the same time.";

      if (contents == GetActiveWebContents())
        EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                                 VIEW_ID_TAB_CONTAINER));
    }

    return true;
  }

  bool ObserveFlashFillColor(SkColor expected_color) const {
    return RunLoopUntil(base::Bind(
        &FlashFullscreenInteractiveBrowserTest::IsObservingFlashFillColor,
        base::Unretained(this),
        expected_color));
  }

 private:
  bool IsObservingTabInFullscreen(content::WebContents* contents,
                                  bool expected_in_fullscreen) const {
    return expected_in_fullscreen ==
           browser()
               ->exclusive_access_manager()
               ->fullscreen_controller()
               ->IsFullscreenForTabOrPending(contents);
  }

  bool IsObservingFlashHasFocus(content::WebContents* contents,
                                bool expected_to_have_focus) const {
    content::RenderWidgetHostView* const flash_fs_view =
        contents->GetFullscreenRenderWidgetHostView();
    const bool flash_has_focus = flash_fs_view && flash_fs_view->HasFocus();
    return flash_has_focus == expected_to_have_focus;
  }

  bool IsObservingActiveWebContents(content::WebContents* contents,
                                    bool expected_active_contents) const {
    return (contents == GetActiveWebContents()) == expected_active_contents;
  }

  bool IsObservingFlashFillColor(SkColor expected_color) const {
    content::RenderWidgetHostView* const flash_fs_view =
        GetActiveWebContents()->GetFullscreenRenderWidgetHostView();
    if (!flash_fs_view) {
      ADD_FAILURE() << "Flash fullscreen RenderWidgetHostView is gone.";
      return false;
    }

    // When a widget is first shown, it can take some time before it is ready
    // for copying from its backing store.  This is a transient condition, and
    // so it is not being treated as a test failure.
    if (!flash_fs_view->IsSurfaceAvailableForCopy())
      return false;

    // Copy and examine the upper-left pixel of the widget and compare it to the
    // |expected_color|.
    bool is_expected_color = false;
    base::RunLoop run_loop;
    flash_fs_view->CopyFromSurface(
        gfx::Rect(0, 0, 1, 1), gfx::Size(1, 1),
        base::BindOnce(
            &FlashFullscreenInteractiveBrowserTest::CheckBitmapForFillColor,
            expected_color, &is_expected_color, run_loop.QuitClosure()));
    run_loop.Run();

    return is_expected_color;
  }

  static void CheckBitmapForFillColor(SkColor expected_color,
                                      bool* is_expected_color,
                                      const base::Closure& done_cb,
                                      const SkBitmap& bitmap) {
    if (!bitmap.drawsNothing())
      *is_expected_color = (bitmap.getColor(0, 0) == expected_color);
    done_cb.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(FlashFullscreenInteractiveBrowserTest);
};

// Tests that launching and exiting fullscreen-within-tab works.
IN_PROC_BROWSER_TEST_F(FlashFullscreenInteractiveBrowserTest,
                       FullscreenWithinTab_EscapeKeyExitsFullscreen) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  StartFakingTabCapture();
  ASSERT_TRUE(LaunchFlashFullscreen());
  content::WebContents* const first_tab_contents = GetActiveWebContents();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, true));
  PressEscape();
  EXPECT_TRUE(ObserveTabIsInFullscreen(false));
}

IN_PROC_BROWSER_TEST_F(FlashFullscreenInteractiveBrowserTest,
                       FullscreenFromSubframe) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  StartFakingTabCapture();
  ASSERT_TRUE(LaunchFlashFullscreenInSubframe());
  content::WebContents* const first_tab_contents = GetActiveWebContents();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, true));
  PressEscape();
  EXPECT_TRUE(ObserveTabIsInFullscreen(false));
}

// This tests that browser UI focus behavior is correct when switching between
// tabs; particularly, that that focus between the omnibox and tab contents is
// stored/restored correctly.  Mouse and keyboard events are used to confirm
// that the widget the UI thinks is focused is the one that responds to these
// input events.
//
// Flaky, see http://crbug.com/444476
IN_PROC_BROWSER_TEST_F(FlashFullscreenInteractiveBrowserTest,
                       DISABLED_FullscreenWithinTab_FocusWhenSwitchingTabs) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  StartFakingTabCapture();
  ASSERT_TRUE(LaunchFlashFullscreen());

  // Upon entering fullscreen, the Flash widget should have focus and be filled
  // with green.
  content::WebContents* const first_tab_contents = GetActiveWebContents();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, true));
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorGREEN));

  // Pressing the spacebar on the keyboard should change the fill color to red
  // to indicate the plugin truly does have the keyboard focus.  Clicking on the
  // view should change the fill color to blue.
  PressSpacebar();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorRED));
  ClickOnTabContainer();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));

  // Launch a new tab.  The Flash widget should have lost focus.
  UseAcceleratorToOpenNewTab();
  content::WebContents* const second_tab_contents = GetActiveWebContents();
  ASSERT_NE(first_tab_contents, second_tab_contents);
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, false));
  ClickOnOmnibox();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, false));
  SpamSpacebar();

  // Switch back to first tab.  The plugin should not have responded to the key
  // presses above (while the omnibox was focused), and should regain focus only
  // now.  Poke it with key and mouse events to confirm.
  UseAcceleratorToSwitchToTab(0);
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, true));
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));
  PressSpacebar();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorRED));
  ClickOnTabContainer();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));

  // Click on the omnibox while still in the first tab, and the Flash widget
  // should lose focus.  Key presses should not affect the color of the Flash
  // widget.
  ClickOnOmnibox();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, false));
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));
  SpamSpacebar();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));

  // Switch to the second tab, click on the web page content, and then go back
  // to the first tab.  Focus should have been restored to the omnibox when
  // going back to the first tab, and so key presses should not change the color
  // of the Flash widget.
  UseAcceleratorToSwitchToTab(1);
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, false));
  ClickOnTabContainer();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, false));
  UseAcceleratorToSwitchToTab(0);
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, false));
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));
  SpamSpacebar();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));

  // Clicking on the Flash widget should give it focus again.
  ClickOnTabContainer();
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, true));
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorRED));
  PressSpacebar();
  ASSERT_TRUE(ObserveFlashFillColor(SK_ColorBLUE));

  // Test that the Escape key is handled as an exit fullscreen command while the
  // Flash widget has the focus.
  EXPECT_TRUE(ObserveFlashHasFocus(first_tab_contents, true));
  PressEscape();
  EXPECT_TRUE(ObserveTabIsInFullscreen(false));
}

// Tests that a fullscreen flash plugin can lock the mouse, and that it'll be
// unlocked when the plugin exits fullscreen.
// Flaky on Linux. See https://crbug.com/706148.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_Fullscreen_LockMouse DISABLED_Fullscreen_LockMouse
#else
#define MAYBE_Fullscreen_LockMouse Fullscreen_LockMouse
#endif
IN_PROC_BROWSER_TEST_F(FlashFullscreenInteractiveBrowserTest,
                       MAYBE_Fullscreen_LockMouse) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  StartFakingTabCapture();
  ASSERT_TRUE(LaunchFlashFullscreen());
  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_TRUE(ObserveFlashHasFocus(web_contents, true));

  // Try to lock the mouse.
  content::RenderWidgetHostView* fullscreen_view =
      web_contents->GetFullscreenRenderWidgetHostView();
  content::RenderWidgetHost* fullscreen_widget =
      fullscreen_view->GetRenderWidgetHost();
  content::RenderProcessHost* process = fullscreen_widget->GetProcess();
  content::PwnMessageHelper::LockMouse(
      process, fullscreen_widget->GetRoutingID(), true, true, false);

  // Make sure that the fullscreen widget got the mouse lock.
  EXPECT_TRUE(fullscreen_view->IsMouseLocked());
  EXPECT_EQ(fullscreen_widget, content::GetMouseLockWidget(web_contents));

  PressEscape();
  EXPECT_TRUE(ObserveTabIsInFullscreen(false));

  // Mouse should be unlocked.
  EXPECT_EQ(nullptr, content::GetMouseLockWidget(web_contents));
}
