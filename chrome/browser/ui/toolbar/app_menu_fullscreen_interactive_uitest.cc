// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/display/screen.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

namespace {
class ViewBoundsChangeWaiter : public views::ViewObserver {
 public:
  explicit ViewBoundsChangeWaiter(views::View* view) {
    observation_.Observe(view);
  }
  ViewBoundsChangeWaiter(const ViewBoundsChangeWaiter&) = delete;
  ViewBoundsChangeWaiter& operator=(const ViewBoundsChangeWaiter&) = delete;

  void Wait() { run_loop_.Run(); }

 private:
  void OnViewBoundsChanged(views::View* view) override { run_loop_.Quit(); }
  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};
}  // namespace

// Tests for Chrome Menu - Fullscreen manual test cases.
class AppMenuFullscreenInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuFullscreenInteractiveTest() = default;
  ~AppMenuFullscreenInteractiveTest() override = default;
  AppMenuFullscreenInteractiveTest(const AppMenuFullscreenInteractiveTest&) =
      delete;
  void operator=(const AppMenuFullscreenInteractiveTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_MAC)
    // SendAccelerator or ui_controls::SendKeyPress doesn't support fn key on
    // Mac, that the default fullscreen hotkey wouldn't work.
    // TODO: When SendAccelerator fixed on mac, remove this hard coded key.
    fullscreen_accelerator_ =
        ui::Accelerator(ui::VKEY_F, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
#else
    chrome::AcceleratorProviderForBrowser(browser())
        ->GetAcceleratorForCommandId(IDC_FULLSCREEN, &fullscreen_accelerator_);
#endif
    chrome::AcceleratorProviderForBrowser(browser())
        ->GetAcceleratorForCommandId(IDC_CLOSE_TAB, &close_tab_accelerator_);
  }

 protected:
  ui::Accelerator fullscreen_accelerator_;
  ui::Accelerator close_tab_accelerator_;

  auto CheckFullscreenForBrowser(bool is_fullscreen) {
    return CheckView(
        kBrowserViewElementId,
        base::BindOnce(
            [](bool is_fullscreen, Browser* browser,
               views::View* browser_view) {
              auto* fullscreen_controller =
                  browser->exclusive_access_manager()->fullscreen_controller();

              // Wait for fullscreen transition complete.
              // Fullscreen transition is an async process on Mac, which
              // browser_window->IsFullscreen() might changed before transition
              // completed.
              if (fullscreen_controller->IsControllerInitiatedFullscreen() !=
                  is_fullscreen) {
                FullscreenNotificationObserver(browser).Wait();
              }
              if (fullscreen_controller->IsControllerInitiatedFullscreen() !=
                  is_fullscreen) {
                return false;
              }

              if (is_fullscreen) {
                do {
                  auto display =
                      display::Screen::GetScreen()->GetDisplayNearestWindow(
                          browser->window()->GetNativeWindow());
                  auto display_size = display.bounds().size();
                  auto workarea_size = display.work_area().size();
                  auto window_size = browser->window()->GetBounds().size();
                  DLOG(INFO) << "display_size = " << display_size.ToString()
                             << " workspace_size = " << workarea_size.ToString()
                             << " window_size = " << window_size.ToString();
                  if (display_size == window_size ||
                      workarea_size == window_size) {
                    return true;
                  }

                  ViewBoundsChangeWaiter(browser_view).Wait();
                } while (true);
              }

              return true;
            },
            is_fullscreen, browser()));
  }
};

// Check Toggle Fullscreen
IN_PROC_BROWSER_TEST_F(AppMenuFullscreenInteractiveTest, ToggleFullscreen) {
  RunTestSequence(
      // P1. Launch Chrome
      // P2. Hit F11/⌘-Ctrl-F/Full screen button
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator_),
      // V2. Make sure chrome is in full screen mode
      CheckFullscreenForBrowser(true),
      // P3. Hit F11/⌘-Ctrl-F/Full screen button again
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator_),
      // V3. Chrome should exit the full screen upon hitting F11 again when in
      // the full screen mode.
      CheckFullscreenForBrowser(false));
}

// Check Full screen Notification
// The original manual test doesn't work on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AppMenuFullscreenInteractiveTest, Notification) {
  RunTestSequence(
      // 1. Launch Chrome
      // 2. Hit F11/⌘-Ctrl-F/Full screen button
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator_),
      // 3. Verify the notification shown on the top in the full screen mode.
      CheckFullscreenForBrowser(true),
      InAnyContext(WaitForShow(kExclusiveAccessBubbleViewElementId)));
}
#endif

// Check Context menu in full screen mode
IN_PROC_BROWSER_TEST_F(AppMenuFullscreenInteractiveTest, ContextMenu) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
  RunTestSequence(
      // 1. Wait for the default page to load.
      InstrumentTab(kPrimaryTabId),
      // 2. Hit F11/⌘-Ctrl-F/Full screen button
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator_),
      // 3. Make sure the chrome window is in full screen mode
      CheckFullscreenForBrowser(true),
      // 4. Right click anywhere on the page to open the context menu. This
      // chooses the center of the browser window, which is fine.
      MoveMouseTo(kBrowserViewElementId), ClickMouse(ui_controls::RIGHT),
      // 5. Make sure context menu is displayed correctly at the expected
      // location when chrome is in full screen mode.
      // WaitForShow and FlushEvents is required to prevent re-enter
      // views::MenuController::OpenMenu from the same call stack during the
      // NotifyElementShown.
      InAnyContext(WaitForShow(RenderViewContextMenu::kExitFullscreenMenuItem)),
      FlushEvents(),
      InAnyContext(
          SelectMenuItem(RenderViewContextMenu::kExitFullscreenMenuItem)),
      CheckFullscreenForBrowser(false));
}

// Check Closing the tab in full screen mode
IN_PROC_BROWSER_TEST_F(AppMenuFullscreenInteractiveTest, ClosingTab) {
  RunTestSequence(
      // 1. Launch Chrome and navigate to few web pages.
      PressButton(kNewTabButtonElementId),
      // 2. Hit F11/⌘-Ctrl-F/Full screen button
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator_),
      CheckFullscreenForBrowser(true),
      // 3. Hit Ctrl+W / Ctrl+F4 [Windows & Linux], ⌘-W [Mac], Ctrl+W [CrOS]
      // when chrome is in the full screen mode. Current tab should be closed
      SendAccelerator(kBrowserViewElementId, close_tab_accelerator_),
      // Chrome should not exit full screen mode when you close a tab in full
      // screen mode.
      CheckFullscreenForBrowser(true));
}
