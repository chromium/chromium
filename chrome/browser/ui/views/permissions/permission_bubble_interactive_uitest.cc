// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"

enum ChipFeatureConfig {
  REQUEST_CHIP,
  REQUEST_CHIP_LOCATION_BAR_ICON_OVERRIDE
};

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
    ASSERT_TRUE(widget);

    views::test::WaitForWidgetActive(widget, true);
  }

  // Send Ctrl/Cmd+keycode in the key window to the browser.
  void SendAcceleratorSync(ui::KeyboardCode keycode, bool shift, bool alt) {
#if BUILDFLAG(IS_MAC)
    bool control = false;
    bool command = true;
#else
    bool control = true;
    bool command = false;
#endif

    // Wait for "key press" instead of "key release" because some tests destroy
    // the target in response to "key press", which prevents "key release" from
    // being observed.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), keycode, control, shift, alt, command,
        /* wait_for=*/ui_controls::KeyEventType::kKeyPress));
  }

  void SetUpOnMainThread() override {
    // Make the browser active (ensures the app can receive key events).
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());

    test_api_->AddSimpleRequest(browser()
                                    ->GetTabStripModel()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame(),
                                permissions::RequestType::kGeolocation);

    EXPECT_TRUE(browser()->GetWindow()->IsActive());

    // The permission prompt is shown asynchronously.
    base::RunLoop().RunUntilIdle();
    OpenBubbleIfRequestChipUiIsShown();

    EnsureWindowActive(test_api_->GetPromptWindow(), "show permission bubble");
  }

  void JumpToNextOpenTab() {
#if BUILDFLAG(IS_MAC)
    SendAcceleratorSync(ui::VKEY_RIGHT, false, true);
#else
    SendAcceleratorSync(ui::VKEY_TAB, false, false);
#endif
  }

  void JumpToPreviousOpenTab() {
#if BUILDFLAG(IS_MAC)
    SendAcceleratorSync(ui::VKEY_LEFT, false, true);
#else
    SendAcceleratorSync(ui::VKEY_TAB, true, false);
#endif
    views::test::RunScheduledLayout(
        BrowserView::GetBrowserViewForBrowser(browser()));
  }

  void OpenBubbleIfRequestChipUiIsShown() {
    // If the permission request is displayed using the chip UI, simulate a
    // click on the chip to trigger showing the prompt.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    LocationBar* lb = browser_view->toolbar()->location_bar();
    // Use the `PermissionChipInterface` rather than looking up a
    // `PermissionChipView`: with the WebUI location bar the chip is a
    // `WebUIPermissionChip` and no views::Button exists in the widget.
    if (lb->GetChipController()->IsPermissionPromptChipVisible() &&
        !lb->GetChipController()->IsBubbleShowing()) {
      lb->GetChipController()->chip()->ExecuteForTesting();
      base::RunLoop().RunUntilIdle();
      // A `WebUIPermissionChip` resolves the bubble's anchor asynchronously,
      // once the WebUI has registered the chip element over Mojo. Wait for it
      // so that the prompt bubble exists by the time this returns.
      ASSERT_TRUE(base::test::RunUntil([lb] {
        return !lb->GetChipController()->is_waiting_for_anchor_for_testing();
      }));
    }
  }

  void TestSwitchingTabsWithCurlyBraces() {
    // Also test switching tabs with curly braces. "VKEY_OEM_4" is
    // LeftBracket/Brace on a US keyboard, which ui::MacKeyCodeForWindowsKeyCode
    // will map to '{' when shift is passed. Also note there are only two tabs
    // so it doesn't matter which direction is taken (it wraps).
    chrome::FocusLocationBar(browser());
    SendAcceleratorSync(ui::VKEY_OEM_4, true, false);
    EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
    OpenBubbleIfRequestChipUiIsShown();
    EnsureWindowActive(test_api_->GetPromptWindow(),
                       "switch to permission tab with curly brace");
    EXPECT_TRUE(test_api_->GetPromptWindow());

    SendAcceleratorSync(ui::VKEY_OEM_4, true, false);
    EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
    browser()->GetWindow()->Activate();
    EnsureWindowActive(browser()->GetWindow(), "switch away with curly brace");
    EXPECT_FALSE(test_api_->GetPromptWindow());
  }

 protected:
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

#if BUILDFLAG(IS_CHROMEOS)
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
  EXPECT_TRUE(browser()->GetWindow()->IsVisible());

  // On Windows, the WM_NCDESTROY message triggering Widget destruction may not
  // have been processed by the time `SendAcceleratorSync` returns (only waits
  // for WM_KEYDOWN). For that reason, wait until there are no more widgets
  // instead of checking immediately that there are no more widgets.
  SendAcceleratorSync(ui::VKEY_W, false, false);
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return views::test::WidgetTest::GetAllWidgets().empty(); }));
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40839289): For Mac builders, the test fails after activating
// the browser and cannot spot the widget. Needs investigation and fix.
#define MAYBE_SwitchTabs DISABLED_SwitchTabs
#else
#define MAYBE_SwitchTabs SwitchTabs
#endif

// Add a tab, ensure we can switch away and back using Ctrl+Tab and
// Ctrl+Shift+Tab at aura and using Cmd+Alt+Left/Right and curly braces at
// MacOS.
IN_PROC_BROWSER_TEST_F(PermissionBubbleInteractiveUITest, MAYBE_SwitchTabs) {
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Add a blank tab in the foreground.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());

#if BUILDFLAG(IS_MAC)
  // The bubble should hide and give focus back to the browser. However, the
  // test environment can't guarantee that macOS decides that the Browser window
  // is actually the "best" window to activate upon closing the current key
  // window. So activate it manually.
  browser()->GetWindow()->Activate();
  EnsureWindowActive(browser()->GetWindow(), "tab added");
#endif

  // Prompt is hidden while its tab is not active.
  EXPECT_FALSE(test_api_->GetPromptWindow());

  // Now a webcontents is active, it gets a first shot at processing the
  // accelerator before sending it back unhandled to the browser via IPC. That's
  // all a bit much to handle in a test, so activate the location bar.
  chrome::FocusLocationBar(browser());

  JumpToPreviousOpenTab();
  // The focused element may live in a renderer (e.g. the WebUI location bar),
  // in which case the accelerator round-trips through the renderer and the tab
  // switch happens asynchronously.
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return browser()->GetTabStripModel()->active_index() == 0; }));

  OpenBubbleIfRequestChipUiIsShown();

  // Note we don't need to makeKeyAndOrderFront for mac os: the permission
  // window will take focus when it is shown again.
  EnsureWindowActive(
      test_api_->GetPromptWindow(),
      "switched to permission tab with ctrl+shift+tab or arrow at mac os");
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Ensure we can switch away with the bubble active.
  JumpToNextOpenTab();
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return browser()->GetTabStripModel()->active_index() == 1; }));

  browser()->GetWindow()->Activate();
  EnsureWindowActive(browser()->GetWindow(),
                     "switch away with ctrl+tab or arrow at mac os");
  EXPECT_FALSE(test_api_->GetPromptWindow());

#if BUILDFLAG(IS_MAC)
  TestSwitchingTabsWithCurlyBraces();
#endif
}

namespace {

// Evaluates to the focused element of the WebUI toolbar, inside shadow roots.
constexpr char kDeepActiveElement[] = R"((() => {
  let active = document.activeElement;
  while (active?.shadowRoot?.activeElement) {
    active = active.shadowRoot.activeElement;
  }
  return active;
})())";

constexpr char kLocationBar[] =
    "document.querySelector('toolbar-app').shadowRoot"
    ".querySelector('#location-bar')";

constexpr char kRequestChipButtonInLocationBar[] =
    ".shadowRoot.querySelector('permission-dashboard')"
    ".shadowRoot.querySelector('#request-chip')"
    ".shadowRoot.querySelector('#chip')";

}  // namespace

// With the WebUI location bar, the permission chip that the prompt bubble is
// anchored to is a DOM element. The bubble should still come right after the
// chip in the sequential focus order, like it does with the Views location
// bar. See crbug.com/580537070.
class PermissionBubbleWebUILocationBarInteractiveUITest
    : public PermissionBubbleInteractiveUITest {
 public:
  PermissionBubbleWebUILocationBarInteractiveUITest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUILocationBar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2},
        {});
  }

 protected:
  content::WebContents* GetToolbarWebContents() {
    WebUIToolbarWebView* const toolbar =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->GetWebUIToolbarViewForTesting();
    CHECK(toolbar);
    return toolbar->GetWebViewForTesting()->GetWebContents();
  }

  // Waits until `script` evaluates to true in the WebUI toolbar.
  [[nodiscard]] bool WaitForToolbarCondition(const std::string& script) {
    return base::test::RunUntil([&] {
      return content::EvalJs(GetToolbarWebContents(), script) == true;
    });
  }

  // Activates the browser window, which leaves the prompt bubble open but
  // inactive, and focuses the omnibox.
  void FocusOmnibox() {
    browser()->GetWindow()->Activate();
    EnsureWindowActive(browser()->GetWindow(), "activate the browser window");
    ASSERT_TRUE(test_api_->GetPromptWindow());

    chrome::FocusLocationBar(browser());
    // Tab is handled by the omnibox while its popup is open.
    ASSERT_TRUE(WaitForToolbarCondition(
        base::StrCat({kDeepActiveElement,
                      "?.getAttribute('part') === 'searchbox-input' && !",
                      kLocationBar, ".isPopupOpen"})));
  }

  void SendTab(bool shift) {
    // Wait for "key press" rather than "key release", since the latter goes
    // to the prompt bubble once it is activated.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_TAB, /*control=*/false, shift, /*alt=*/false,
        /*command=*/false, ui_controls::KeyEventType::kKeyPress));
  }

  // Waits until the prompt bubble is active, and checks that its first
  // focusable view (or its last one, if `reverse` is true) has focus.
  void ExpectFocusInPromptBubble(bool reverse, const char* message) {
    views::Widget* const prompt = test_api_->GetPromptWindow();
    ASSERT_TRUE(prompt);
    EnsureWindowActive(prompt, message);
    views::FocusManager* const focus_manager = prompt->GetFocusManager();
    views::View* const expected = focus_manager->GetNextFocusableView(
        /*starting_view=*/nullptr, prompt, reverse, /*dont_loop=*/true);
    ASSERT_TRUE(expected);
    EXPECT_EQ(expected, focus_manager->GetFocusedView()) << message;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionBubbleWebUILocationBarInteractiveUITest,
                       TabFromChipFocusesBubble) {
  FocusOmnibox();
  const std::string chip_button =
      base::StrCat({kLocationBar, kRequestChipButtonInLocationBar});
  ASSERT_TRUE(content::ExecJs(GetToolbarWebContents(),
                              base::StrCat({chip_button, ".focus();"})));
  ASSERT_TRUE(WaitForToolbarCondition(
      base::StrCat({kDeepActiveElement, " === ", chip_button})));

  SendTab(/*shift=*/false);
  ExpectFocusInPromptBubble(/*reverse=*/false, "Tab from the chip");
}

// The omnibox comes right after the chip, so Shift+Tab from it enters the
// bubble from the end.
IN_PROC_BROWSER_TEST_F(PermissionBubbleWebUILocationBarInteractiveUITest,
                       ShiftTabFromOmniboxFocusesBubble) {
  FocusOmnibox();

  SendTab(/*shift=*/true);
  ExpectFocusInPromptBubble(/*reverse=*/true, "Shift+Tab from the omnibox");
}

// Closing the bubble gives focus back to the omnibox, which is where it was
// before it moved into the bubble.
IN_PROC_BROWSER_TEST_F(PermissionBubbleWebUILocationBarInteractiveUITest,
                       ClosingBubbleReturnsFocusToOmnibox) {
  FocusOmnibox();

  SendTab(/*shift=*/true);
  ExpectFocusInPromptBubble(/*reverse=*/true, "Shift+Tab from the omnibox");

  test_api_->GetPromptWindow()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  EnsureWindowActive(browser()->GetWindow(), "close the prompt bubble");
  EXPECT_TRUE(WaitForToolbarCondition(
      base::StrCat({"document.hasFocus() && ", kDeepActiveElement,
                    "?.getAttribute('part') === 'searchbox-input'"})));
}
