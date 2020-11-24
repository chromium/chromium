// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class PermissionBubbleInteractiveUITest : public InProcessBrowserTest {
 public:
  PermissionBubbleInteractiveUITest() = default;

  PermissionBubbleInteractiveUITest(const PermissionBubbleInteractiveUITest&) =
      delete;
  PermissionBubbleInteractiveUITest& operator=(
      const PermissionBubbleInteractiveUITest&) = delete;

  void EnsureWindowActive(ui::BaseWindow* window, const char* message) {
    EnsureWindowActive(
        views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow()),
        message);
  }

  void EnsureWindowActive(views::Widget* widget, const char* message) {
    SCOPED_TRACE(message);
    EXPECT_TRUE(widget);

    views::test::WidgetActivationWaiter waiter(widget, true);
    waiter.Wait();
  }

  // Send Ctrl/Cmd+keycode in the key window to the browser.
  void SendAcceleratorSync(ui::KeyboardCode keycode, bool shift, bool alt) {
#if defined(OS_MAC)
    bool control = false;
    bool command = true;
#else
    bool control = true;
    bool command = false;
#endif

    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), keycode, control,
                                                shift, alt, command));
  }

  void SetUpOnMainThread() override {
    // Make the browser active (ensures the app can receive key events).
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());

    test_api_->AddSimpleRequest(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        ContentSettingsType::GEOLOCATION);

    EXPECT_TRUE(browser()->window()->IsActive());

    // The permission prompt is shown asynchronously.
    base::RunLoop().RunUntilIdle();
    OpenBubbleIfChipUiIsShown();

    EnsureWindowActive(test_api_->GetPromptWindow(), "show permission bubble");
  }

  void JumpToNextOpenTab() {
#if defined(OS_MAC)
    SendAcceleratorSync(ui::VKEY_RIGHT, false, true);
#else
    SendAcceleratorSync(ui::VKEY_TAB, false, false);
#endif
  }

  void JumpToPreviousOpenTab() {
#if defined(OS_MAC)
    SendAcceleratorSync(ui::VKEY_LEFT, false, true);
#else
    SendAcceleratorSync(ui::VKEY_TAB, true, false);
#endif
  }

  void OpenBubbleIfChipUiIsShown() {
    // If the permission request is displayed using the chip UI, simulate a
    // click on the chip to trigger showing the prompt.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PermissionChip* permission_chip =
        browser_view->toolbar()->location_bar()->permission_chip();
    if (permission_chip->GetVisible()) {
      views::test::ButtonTestApi(permission_chip->button())
          .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                      gfx::Point(), ui::EventTimeForNow(),
                                      ui::EF_LEFT_MOUSE_BUTTON, 0));
      base::RunLoop().RunUntilIdle();
    }
  }

  void TestSwitchingTabsWithCurlyBraces() {
    // Also test switching tabs with curly braces. "VKEY_OEM_4" is
    // LeftBracket/Brace on a US keyboard, which ui::MacKeyCodeForWindowsKeyCode
    // will map to '{' when shift is passed. Also note there are only two tabs
    // so it doesn't matter which direction is taken (it wraps).
    chrome::FocusLocationBar(browser());
    SendAcceleratorSync(ui::VKEY_OEM_4, true, false);
    EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
    OpenBubbleIfChipUiIsShown();
    EnsureWindowActive(test_api_->GetPromptWindow(),
                       "switch to permission tab with curly brace");
    EXPECT_TRUE(test_api_->GetPromptWindow());

    SendAcceleratorSync(ui::VKEY_OEM_4, true, false);
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
    browser()->window()->Activate();
    EnsureWindowActive(browser()->window(), "switch away with curly brace");
    EXPECT_FALSE(test_api_->GetPromptWindow());
  }

 protected:
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/1072425): views::test::WidgetTest::GetAllWidgets() crashes
// on Chrome OS, need to investigate\fix that.
#define MAYBE_CmdWClosesWindow DISABLED_CmdWClosesWindow
#else
#define MAYBE_CmdWClosesWindow CmdWClosesWindow
#endif

// There is only one tab. Ctrl/Cmd+w will close it along with the browser
// window.
IN_PROC_BROWSER_TEST_F(PermissionBubbleInteractiveUITest,
                       MAYBE_CmdWClosesWindow) {
  EXPECT_TRUE(browser()->window()->IsVisible());

  SendAcceleratorSync(ui::VKEY_W, false, false);

  // The window has been destroyed so there should be no widgets hanging around.
  EXPECT_EQ(0u, views::test::WidgetTest::GetAllWidgets().size());
}

// Add a tab, ensure we can switch away and back using Ctrl+Tab and
// Ctrl+Shift+Tab at aura and using Cmd+Alt+Left/Right and curly braces at
// MacOS.
IN_PROC_BROWSER_TEST_F(PermissionBubbleInteractiveUITest, SwitchTabs) {
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Add a blank tab in the foreground.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

#if defined(OS_MAC)
  // The bubble should hide and give focus back to the browser. However, the
  // test environment can't guarantee that macOS decides that the Browser window
  // is actually the "best" window to activate upon closing the current key
  // window. So activate it manually.
  browser()->window()->Activate();
  EnsureWindowActive(browser()->window(), "tab added");
#endif

  // Prompt is hidden while its tab is not active.
  EXPECT_FALSE(test_api_->GetPromptWindow());

  // Now a webcontents is active, it gets a first shot at processing the
  // accelerator before sending it back unhandled to the browser via IPC. That's
  // all a bit much to handle in a test, so activate the location bar.
  chrome::FocusLocationBar(browser());

  JumpToPreviousOpenTab();
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  OpenBubbleIfChipUiIsShown();

  // Note we don't need to makeKeyAndOrderFront for mac os: the permission
  // window will take focus when it is shown again.
  EnsureWindowActive(
      test_api_->GetPromptWindow(),
      "switched to permission tab with ctrl+shift+tab or arrow at mac os");
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Ensure we can switch away with the bubble active.
  JumpToNextOpenTab();
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  browser()->window()->Activate();
  EnsureWindowActive(browser()->window(),
                     "switch away with ctrl+tab or arrow at mac os");
  EXPECT_FALSE(test_api_->GetPromptWindow());

#if defined(OS_MAC)
  TestSwitchingTabsWithCurlyBraces();
#endif
}
