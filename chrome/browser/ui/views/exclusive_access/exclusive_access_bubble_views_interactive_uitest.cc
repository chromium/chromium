// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/exclusive_access/exclusive_access_bubble_views.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

// The menu's contents are irrelevant to these tests; all that matters is that
// a menu is open and therefore capable of covering the bubble.
class TestMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  void ExecuteCommand(int command_id, int event_flags) override {}
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return false;
  }
};

}  // namespace

class ExclusiveAccessBubbleViewsTest : public ExclusiveAccessTest,
                                       public views::WidgetObserver {
 public:
  ExclusiveAccessBubbleViewsTest() = default;

  ExclusiveAccessBubbleViewsTest(const ExclusiveAccessBubbleViewsTest&) =
      delete;
  ExclusiveAccessBubbleViewsTest& operator=(
      const ExclusiveAccessBubbleViewsTest&) = delete;

  void TearDownOnMainThread() override {
    CloseMenu();
    ExclusiveAccessTest::TearDownOnMainThread();
  }

  ExclusiveAccessBubbleViewsContext* GetContext() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetExclusiveAccessBubbleViewsContextForTesting();
  }

  void ClearSnooze() {
    GetExclusiveAccessBubbleView()->snooze_until_ = base::TimeTicks::Min();
  }

  void ClearMustShowOnNextInteraction() {
    GetExclusiveAccessBubbleView()->must_show_next_interaction_ = false;
  }

  bool MustShowOnNextInteraction() {
    return GetExclusiveAccessBubbleView()->must_show_next_interaction_;
  }

  // Opens a menu owned by the browser window. Views menus are asynchronous, so
  // this returns while the menu is still open.
  void OpenMenu() {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(&menu_delegate_);
    menu_model_->AddItem(/*command_id=*/1, u"Item");
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
    views::Widget* const widget =
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
    menu_runner_->RunMenuAt(
        widget, /*button_controller=*/nullptr,
        gfx::Rect(widget->GetWindowBoundsInScreen().CenterPoint(), gfx::Size()),
        views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kMouse);
  }

  void CloseMenu() {
    if (menu_runner_) {
      menu_runner_->Cancel();
      menu_runner_.reset();
      menu_model_.reset();
    }
  }

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    was_observing_in_destroying_ =
        widget->HasObserver(GetExclusiveAccessBubbleView());
    was_destroying_ = true;
    widget->RemoveObserver(this);
  }

 protected:
  bool was_destroying_ = false;
  bool was_observing_in_destroying_ = false;

 private:
  TestMenuDelegate menu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

// Simulate obscure codepaths resulting in the bubble Widget being closed before
// the ExclusiveAccessBubbleViews destructor asks for it. If a close bypasses
// the destructor, animations could still be running that attempt to manipulate
// a destroyed Widget and crash.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, NativeClose) {
  EXPECT_FALSE(GetExclusiveAccessBubbleView());
  EnterActiveTabFullscreen();
  EXPECT_TRUE(GetExclusiveAccessBubbleView());

  GetExclusiveAccessBubbleView()->GetView()->GetWidget()->AddObserver(this);

  // Simulate the bubble being closed out from under its controller, which seems
  // to happen in some odd corner cases, like system log-off while the bubble is
  // showing.
  GetExclusiveAccessBubbleView()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(GetExclusiveAccessBubbleView());

  // Verify that teardown is really happening via OnWidgetDestroyed() rather
  // than the usual path via the ExclusiveAccessBubbleViews destructor. Since
  // the destructor always first removes ExclusiveAccessBubbleViews as an
  // observer before starting the close, checking in OnWidgetDestroyed that it's
  // still observing achieves this.
  EXPECT_TRUE(was_observing_in_destroying_);
  EXPECT_TRUE(was_destroying_);
}

// Tests that creating an exclusive access bubble for a download does not crash,
// despite the type being EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE. See
// crbug.com/40278531.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, CreateForDownload) {
  ExclusiveAccessBubbleViews bubble(GetContext(), {.has_download = true},
                                    base::NullCallback());
  EXPECT_TRUE(IsBubbleDownloadNotification(&bubble));
}

// Ensure the bubble reshows on mouse move events after a suppression period.
// TODO(crbug.com/336399260): Enable on macOS
// TODO(crbug.com/372814576): Enable on Wayland
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReshowOnMove DISABLED_ReshowOnMove
#else
#define MAYBE_ReshowOnMove ReshowOnMove
#endif
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, MAYBE_ReshowOnMove) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Skipping for Wayland";
  }
#endif
  // Click on the tab now, so test events are sent to that target later.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Show the bubble, wait for it to hide, and clear the 15min snooze signal.
  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());

    ExclusiveAccessBubbleHideCallback callback =
        base::BindLambdaForTesting([&](ExclusiveAccessBubbleHideReason reason) {
          EXPECT_EQ(reason, ExclusiveAccessBubbleHideReason::kTimeout);
        });
    GetExclusiveAccessManager()->context()->UpdateExclusiveAccessBubble(
        {.origin = url::Origin::Create(GURL("http://example.com")),
         .type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION},
        std::move(callback));
    EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());

    task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime * 2);
  }

  FinishExclusiveAccessBubbleAnimation();
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ClearSnooze();

  // The bubble reshows on a mouse move event.
  const auto point = BrowserView::GetBrowserViewForBrowser(browser())
                         ->GetBoundsInScreen()
                         .CenterPoint();
  base::RunLoop move_run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(point.x(), point.y(),
                                           move_run_loop.QuitClosure());
  move_run_loop.Run();
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

// Ensure the bubble reshows on mouse click events after a suppression period.
// TODO(crbug.com/336399260): Enable on macOS
// TODO(crbug.com/372814576): Enable on Wayland
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReshowOnClick DISABLED_ReshowOnClick
#else
#define MAYBE_ReshowOnClick ReshowOnClick
#endif
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, MAYBE_ReshowOnClick) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Skipping for Wayland";
  }
#endif
  // Click on the tab now, so test events are sent to that target later.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Show the bubble, wait for it to hide, and clear the 15min snooze signal.
  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());

    ExclusiveAccessBubbleHideCallback callback =
        base::BindLambdaForTesting([&](ExclusiveAccessBubbleHideReason reason) {
          EXPECT_EQ(reason, ExclusiveAccessBubbleHideReason::kTimeout);
        });

    GetExclusiveAccessManager()->context()->UpdateExclusiveAccessBubble(
        {.origin = url::Origin::Create(GURL("http://example.com")),
         .type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION},
        std::move(callback));
    EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());

    task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime * 2);
  }

  FinishExclusiveAccessBubbleAnimation();
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ClearSnooze();

  // The bubble reshows on a mouse click event; avoid sending a move event.
  base::RunLoop click_run_loop;
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN, click_run_loop.QuitClosure());
  click_run_loop.Run();
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

// TODO(crbug.com/528276492): Reenable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PresentationWatchdog DISABLED_PresentationWatchdog
#else
#define MAYBE_PresentationWatchdog PresentationWatchdog
#endif
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest,
                       MAYBE_PresentationWatchdog) {
  ExclusiveAccessBubbleViews::set_simulate_gpu_hang_for_testing(true);

  // Enter fullscreen. This should trigger bubble creation.
  EnterActiveTabFullscreen();
  EXPECT_TRUE(GetExclusiveAccessBubbleView());
  EXPECT_TRUE(IsFullscreenForBrowser() || IsWindowFullscreenForTabOrPending());

  // Wait for the watchdog to fire (timeout is 1.5s).
  Wait(base::Milliseconds(2000));

  // The watchdog should have fired, and we should have exited fullscreen.
  EXPECT_FALSE(IsFullscreenForBrowser() || IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(GetExclusiveAccessBubbleView());

  // Clean up.
  ExclusiveAccessBubbleViews::set_simulate_gpu_hang_for_testing(false);
}

// This test is Windows-only because it tests Win32-specific pointer-lock
// behavior (unadjusted movement drifting cursor to screen boundaries causing
// DWM caption click interception) and utilizes Win32-specific APIs
// (GetCursorPos).
#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest,
                       PointerLockUnadjustedMovementFullscreenClickAtTop) {
  // Navigate to a blank page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Focus the tab content.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Enter tab fullscreen.
  EnterActiveTabFullscreen();
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Set up listeners for lock and mouse events.
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    window.lockStatus = '';
    window.mousedownCount = 0;
    window.mouseupCount = 0;
    window.addEventListener('mousedown', () => { window.mousedownCount++; });
    window.addEventListener('mouseup', () => { window.mouseupCount++; });
    document.addEventListener('click', () => {
      document.body.requestPointerLock({unadjustedMovement: true})
        .then(() => { window.lockStatus = 'success'; })
        .catch((e) => { window.lockStatus = 'error: ' + e.name; });
    }, {once: true});
  )"));

  // Send a click in the center of the window to trigger pointer lock.
  gfx::Rect window_bounds =
      BrowserView::GetBrowserViewForBrowser(browser())->GetBoundsInScreen();
  gfx::Point center_point = window_bounds.CenterPoint();
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(center_point));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP));

  // Wait for pointer lock to be acquired.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !content::EvalJs(web_contents, "window.lockStatus")
                .ExtractString()
                .empty();
  }));
  EXPECT_EQ(content::EvalJs(web_contents, "window.lockStatus").ExtractString(),
            "success");

  // Move the mouse to the top edge of the screen (y = bounds.y()).
  gfx::Point top_point(window_bounds.CenterPoint().x(), window_bounds.y());
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(top_point));

  // Check the physical cursor position.
  // The cursor should have drifted to the top and been clipped by ClipCursor
  // (which insets by 5 pixels).
  POINT cursor_pos;
  ASSERT_TRUE(::GetCursorPos(&cursor_pos));
  EXPECT_LE(cursor_pos.y, 5);

  // Click at the top edge.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP));

  // Check if mouse down and up events were registered on the page.
  // With the OnNCHitTest fix, the click should be routed to the client area
  // and reach the page.
  EXPECT_EQ(
      2, content::EvalJs(web_contents, "window.mousedownCount").ExtractInt());
  EXPECT_EQ(2,
            content::EvalJs(web_contents, "window.mouseupCount").ExtractInt());
}
#endif

// Menus are excluded on Mac because `MenuRunner` runs a blocking native menu
// there, and because the fullscreen transition only cancels menus once the
// AppKit animation completes. See crbug.com/40060516.
#if !BUILDFLAG(IS_MAC)

// The exit instruction must not be covered by a menu when a page takes the
// screen. `MenuController` cancels menus when the browser widget's show state
// changes, which covers this case today, but that is incidental; this test
// pins the behavior down.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest,
                       MenuDismissedEnteringTabFullscreen) {
  OpenMenu();
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  EnterActiveTabFullscreen();
  ASSERT_TRUE(GetExclusiveAccessBubbleView());

  EXPECT_FALSE(views::MenuController::GetActiveInstance());
}

// As above, but the window is already fullscreen when the page requests
// fullscreen. No show state change occurs in that case, so the bubble itself is
// responsible for dismissing the menu.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest,
                       MenuDismissedEnteringTabFullscreenWhileFullscreen) {
  GetFullscreenController()->ToggleBrowserFullscreenMode(
      /*user_initiated=*/true);
  WaitAndVerifyFullscreenState(/*browser_fullscreen=*/true,
                               /*tab_fullscreen=*/false);

  OpenMenu();
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  EnterActiveTabFullscreen();
  ASSERT_TRUE(GetExclusiveAccessBubbleView());

  EXPECT_FALSE(views::MenuController::GetActiveInstance());
}

// A menu that opens after the bubble is already showing can cover it for the
// bubble's entire lifetime. Such a show must not consume `kSnoozeTime`, which
// would otherwise suppress the exit instruction for the next 15 minutes.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest,
                       ObscuredBubbleReshowsOnNextInteraction) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(GetExclusiveAccessBubbleView());

  // `EnterActiveTabFullscreen()` does not deliver a real user gesture, so the
  // bubble already arms a re-show. Clear it to observe the menu's effect alone.
  ClearMustShowOnNextInteraction();
  ASSERT_FALSE(MustShowOnNextInteraction());

  OpenMenu();
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // Let the show time elapse, so the bubble hides while the menu is still open.
  Wait(ExclusiveAccessBubble::kShowTime * 2);
  ASSERT_TRUE(GetExclusiveAccessBubbleView());

  EXPECT_TRUE(MustShowOnNextInteraction());
}

// Counterpart to the above: an unobscured show is allowed to consume the snooze
// period as usual.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest,
                       UnobscuredBubbleDoesNotReshowOnNextInteraction) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(GetExclusiveAccessBubbleView());

  ClearMustShowOnNextInteraction();
  ASSERT_FALSE(MustShowOnNextInteraction());

  Wait(ExclusiveAccessBubble::kShowTime * 2);
  ASSERT_TRUE(GetExclusiveAccessBubbleView());
  ASSERT_FALSE(views::MenuController::GetActiveInstance());

  EXPECT_FALSE(MustShowOnNextInteraction());
}

#endif  // !BUILDFLAG(IS_MAC)
