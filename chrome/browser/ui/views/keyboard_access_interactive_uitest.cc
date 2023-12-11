// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// An async version of SendKeyPressSync since we don't get notified when a
// menu is showing.
void SendKeyPress(Browser* browser, ui::KeyboardCode key) {
  ASSERT_TRUE(ui_controls::SendKeyPress(browser->window()->GetNativeWindow(),
                                        key, false, false, false, false));
}

// Helper class that waits until the focus has changed to a view other
// than the one with the provided view id.
class ViewFocusChangeWaiter : public views::FocusChangeListener {
 public:
  ViewFocusChangeWaiter(views::FocusManager* focus_manager,
                        int previous_view_id)
      : focus_manager_(focus_manager), previous_view_id_(previous_view_id) {
    focus_manager_->AddFocusChangeListener(this);
    // Call the focus change notification once in case the focus has
    // already changed.
    OnWillChangeFocus(nullptr, focus_manager_->GetFocusedView());
  }

  ViewFocusChangeWaiter(const ViewFocusChangeWaiter&) = delete;
  ViewFocusChangeWaiter& operator=(const ViewFocusChangeWaiter&) = delete;

  ~ViewFocusChangeWaiter() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void Wait() { loop_.Run(); }

 private:
  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (focused_now && focused_now->GetID() != previous_view_id_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, loop_.QuitWhenIdleClosure());
    }
  }

  raw_ptr<views::FocusManager> focus_manager_;
  base::RunLoop loop_;
  int previous_view_id_;
  base::WeakPtrFactory<ViewFocusChangeWaiter> weak_factory_{this};
};

class SendKeysMenuListener : public AppMenuButtonObserver {
 public:
  SendKeysMenuListener(AppMenuButton* app_menu_button,
                       Browser* browser,
                       bool test_dismiss_menu)
      : browser_(browser),
        menu_open_count_(0),
        test_dismiss_menu_(test_dismiss_menu) {
    observation_.Observe(app_menu_button);
  }

  SendKeysMenuListener(const SendKeysMenuListener&) = delete;
  SendKeysMenuListener& operator=(const SendKeysMenuListener&) = delete;

  ~SendKeysMenuListener() override = default;

  void Wait() { loop_.Run(); }

  // AppMenuButtonObserver:
  void AppMenuShown() override {
    menu_open_count_++;
    if (test_dismiss_menu_) {
      SendKeyPress(browser_, ui::VKEY_ESCAPE);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, loop_.QuitWhenIdleClosure(), base::Milliseconds(200));
    } else {
      DCHECK(observation_.IsObserving());
      observation_.Reset();
      // Press DOWN to select the first item, then RETURN to select it.
      SendKeyPress(browser_, ui::VKEY_DOWN);
      SendKeyPress(browser_, ui::VKEY_RETURN);
    }
  }

  int menu_open_count() const { return menu_open_count_; }

 private:
  raw_ptr<Browser> browser_;
  // Keeps track of the number of times the menu was opened.
  int menu_open_count_;
  // If this is set then on receiving a notification that the menu was opened
  // we dismiss it by sending the ESC key.
  bool test_dismiss_menu_;

  // This used to use content::RunMessageLoop() which used a nestable loop. We
  // tried removing kNestableTasksAllowed but that failed on trybots.
  base::RunLoop loop_{base::RunLoop::Type::kNestableTasksAllowed};

  base::ScopedObservation<AppMenuButton, AppMenuButtonObserver> observation_{
      this};
};

class KeyboardAccessTest : public InProcessBrowserTest {
 public:
  KeyboardAccessTest() {}

  KeyboardAccessTest(const KeyboardAccessTest&) = delete;
  KeyboardAccessTest& operator=(const KeyboardAccessTest&) = delete;

  // Use the keyboard to select "New Tab" from the app menu.
  // This test depends on the fact that there is one menu and that
  // New Tab is the first item in the menu. If the menus change,
  // this test will need to be changed to reflect that.
  //
  // If alternate_key_sequence is true, use "Alt" instead of "F10" to
  // open the menu bar, and "Down" instead of "Enter" to open a menu.
  // If focus_omnibox is true then the test on startup sets focus to the
  // omnibox.
  void TestMenuKeyboardAccess(bool alternate_key_sequence,
                              bool shift,
                              bool focus_omnibox);

  int GetFocusedViewID() {
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    const views::FocusManager* focus_manager = widget->GetFocusManager();
    const views::View* focused_view = focus_manager->GetFocusedView();
    return focused_view ? focused_view->GetID() : -1;
  }

  void WaitForFocusedViewIDToChange(int original_view_id) {
    if (GetFocusedViewID() != original_view_id) {
      return;
    }
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    views::FocusManager* focus_manager = widget->GetFocusManager();
    ViewFocusChangeWaiter waiter(focus_manager, original_view_id);
    waiter.Wait();
  }

#if BUILDFLAG(IS_WIN)
  // Opens the system menu on Windows with the Alt Space combination and selects
  // the New Tab option from the menu.
  void TestSystemMenuWithKeyboard();
  void TestSystemMenuReopenClosedTabWithKeyboard();
#endif

  // Uses the keyboard to select the app menu i.e. with the F10 key.
  // It verifies that the menu when dismissed by sending the ESC key it does
  // not display twice.
  void TestMenuKeyboardAccessAndDismiss();
};

void KeyboardAccessTest::TestMenuKeyboardAccess(bool alternate_key_sequence,
                                                bool shift,
                                                bool focus_omnibox) {
  // Navigate to a page in the first tab, which makes sure that focus is
  // set to the browser window.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

  // The initial tab index should be 0.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Get the focused view ID, then press a key to activate the
  // page menu, then wait until the focused view changes.
  int original_view_id = GetFocusedViewID();

  ui_test_utils::TabAddedWaiter tab_add(browser());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  SendKeysMenuListener menu_listener(
      browser_view->toolbar_button_provider()->GetAppMenuButton(), browser(),
      false);

  if (focus_omnibox) {
    browser()->window()->GetLocationBar()->FocusLocation(false);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS doesn't have a way to just focus the app menu, so we use Alt+F to
  // bring up the menu.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F, false,
                                              shift, true, false));
#else
  ui::KeyboardCode menu_key =
      alternate_key_sequence ? ui::VKEY_MENU : ui::VKEY_F10;
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), menu_key, false, shift,
                                              false, false));
#endif

  if (shift) {
    // Verify Chrome does not move the view focus. We should not move the view
    // focus when typing a menu key with modifier keys, such as shift keys or
    // control keys.
    int new_view_id = GetFocusedViewID();
    ASSERT_EQ(original_view_id, new_view_id);
    return;
  }

  WaitForFocusedViewIDToChange(original_view_id);

  // See above comment. Since we already brought up the menu, no need to do this
  // on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (alternate_key_sequence) {
    SendKeyPress(browser(), ui::VKEY_DOWN);
  } else {
    SendKeyPress(browser(), ui::VKEY_RETURN);
  }
#endif

  // Wait for the new tab to appear.
  tab_add.Wait();

  // Make sure that the new tab index is 1.
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
}

#if BUILDFLAG(IS_WIN)

// This CBT hook is set for the duration of the TestSystemMenuWithKeyboard test
LRESULT CALLBACK SystemMenuTestCBTHook(int n_code,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  // Look for the system menu window getting created or becoming visible and
  // then select the New Tab option from the menu.
  if (n_code == HCBT_ACTIVATE || n_code == HCBT_CREATEWND) {
    wchar_t class_name[MAX_PATH] = {0};
    GetClassName(reinterpret_cast<HWND>(w_param), class_name,
                 std::size(class_name));
    if (base::EqualsCaseInsensitiveASCII(class_name, "#32768")) {
      // Select the New Tab option and then send the enter key to execute it.
      ::PostMessage(reinterpret_cast<HWND>(w_param), WM_CHAR, 'T', 0);
      ::PostMessage(reinterpret_cast<HWND>(w_param), WM_KEYDOWN, VK_RETURN, 0);
      ::PostMessage(reinterpret_cast<HWND>(w_param), WM_KEYUP, VK_RETURN, 0);
    }
  }
  return ::CallNextHookEx(0, n_code, w_param, l_param);
}

void KeyboardAccessTest::TestSystemMenuWithKeyboard() {
  // Navigate to a page in the first tab, which makes sure that focus is
  // set to the browser window.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::TabAddedWaiter tab_add(browser());
  // Sending the Alt space keys to the browser will bring up the system menu
  // which runs a model loop. We set a CBT hook to look for the menu and send
  // keystrokes to it.
  HHOOK cbt_hook = ::SetWindowsHookEx(WH_CBT, SystemMenuTestCBTHook, NULL,
                                      ::GetCurrentThreadId());
  ASSERT_TRUE(cbt_hook);

  bool ret = ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_SPACE, false,
                                             false, true, false);
  EXPECT_TRUE(ret);

  if (ret) {
    // Wait for the new tab to appear.
    tab_add.Wait();
    // Make sure that the new tab index is 1.
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  }
  ::UnhookWindowsHookEx(cbt_hook);
}

// This CBT hook is set for the duration of the
// TestSystemMenuReopenClosedTabWithKeyboard test
LRESULT CALLBACK SystemMenuReopenClosedTabTestCBTHook(int n_code,
                                                      WPARAM w_param,
                                                      LPARAM l_param) {
  // Look for the system menu window getting created or becoming visible and
  // then select the New Tab option from the menu.
  if (n_code == HCBT_ACTIVATE || n_code == HCBT_CREATEWND) {
    wchar_t class_name[MAX_PATH] = {0};
    GetClassName(reinterpret_cast<HWND>(w_param), class_name,
                 std::size(class_name));
    if (base::EqualsCaseInsensitiveASCII(class_name, "#32768")) {
      // Send 'E' for the Reopen closed tab option.
      ::PostMessage(reinterpret_cast<HWND>(w_param), WM_CHAR, 'E', 0);
    }
  }
  return ::CallNextHookEx(0, n_code, w_param, l_param);
}

void KeyboardAccessTest::TestSystemMenuReopenClosedTabWithKeyboard() {
  // Navigate to a page in the first tab, which makes sure that focus is
  // set to the browser window.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://version/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  content::WebContents* tab_to_close =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab_to_close);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::TabAddedWaiter tab_add(browser());
  // Sending the Alt space keys to the browser will bring up the system menu
  // which runs a model loop. We set a CBT hook to look for the menu and send
  // keystrokes to it.
  HHOOK cbt_hook =
      ::SetWindowsHookEx(WH_CBT, SystemMenuReopenClosedTabTestCBTHook, NULL,
                         ::GetCurrentThreadId());
  ASSERT_TRUE(cbt_hook);

  bool ret = ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_SPACE, false,
                                             false, true, false);
  EXPECT_TRUE(ret);

  if (ret) {
    // Wait for the new tab to appear.
    tab_add.Wait();
    // Make sure that the new tab index is 1.
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  }

  ::UnhookWindowsHookEx(cbt_hook);
}
#endif

void KeyboardAccessTest::TestMenuKeyboardAccessAndDismiss() {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  int original_view_id = GetFocusedViewID();

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  SendKeysMenuListener menu_listener(
      browser_view->toolbar_button_provider()->GetAppMenuButton(), browser(),
      true);
  browser()->window()->GetLocationBar()->FocusLocation(false);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F10, false,
                                              false, false, false));

  WaitForFocusedViewIDToChange(original_view_id);

  SendKeyPress(browser(), ui::VKEY_DOWN);
  menu_listener.Wait();
  ASSERT_EQ(1, menu_listener.menu_open_count());
}

// http://crbug.com/62310.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TestMenuKeyboardAccess DISABLED_TestMenuKeyboardAccess
#elif BUILDFLAG(IS_MAC)
// No keyboard shortcut for the Chrome menu on Mac: http://crbug.com/823952
#define MAYBE_TestMenuKeyboardAccess DISABLED_TestMenuKeyboardAccess
#else
#define MAYBE_TestMenuKeyboardAccess TestMenuKeyboardAccess
#endif

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_TestMenuKeyboardAccess) {
  TestMenuKeyboardAccess(false, false, false);
}

// http://crbug.com/62310.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TestAltMenuKeyboardAccess DISABLED_TestAltMenuKeyboardAccess
#elif BUILDFLAG(IS_MAC)
// No keyboard shortcut for the Chrome menu on Mac: http://crbug.com/823952
#define MAYBE_TestAltMenuKeyboardAccess DISABLED_TestAltMenuKeyboardAccess
#else
#define MAYBE_TestAltMenuKeyboardAccess TestAltMenuKeyboardAccess
#endif
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_TestAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, false, false);
}

// If this flakes, use http://crbug.com/62311.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestShiftAltMenuKeyboardAccess \
  DISABLED_TestShiftAltMenuKeyboardAccess
#else
#define MAYBE_TestShiftAltMenuKeyboardAccess TestShiftAltMenuKeyboardAccess
#endif
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest,
                       MAYBE_TestShiftAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, true, false);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest,
                       DISABLED_TestAltMenuKeyboardAccessFocusOmnibox) {
  TestMenuKeyboardAccess(true, false, true);
}

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, TestSystemMenuWithKeyboard) {
  TestSystemMenuWithKeyboard();
}

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest,
                       TestSystemMenuReopenClosedTabWithKeyboard) {
  TestSystemMenuReopenClosedTabWithKeyboard();
}
#endif

#if !BUILDFLAG(IS_WIN) && defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, TestMenuKeyboardOpenDismiss) {
  TestMenuKeyboardAccessAndDismiss();
}
#endif

// Test that JavaScript cannot intercept reserved keyboard accelerators like
// ctrl-t to open a new tab or ctrl-f4 to close a tab.
// TODO(isherman): This test times out on ChromeOS.  We should merge it with
// BrowserKeyEventsTest.ReservedAccelerators, but just disable for now.
// If this flakes, use http://crbug.com/62311.
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, ReserveKeyboardAccelerators) {
  const std::string kBadPage =
      "<html><script>"
      "document.onkeydown = function() {"
      "  event.preventDefault();"
      "  return false;"
      "}"
      "</script></html>";
  GURL url("data:text/html," + kBadPage);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, true,
                                              false, false, false));
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
#if BUILDFLAG(IS_MAC)
      browser(), ui::VKEY_W, false, false, false, true));
#else
      browser(), ui::VKEY_W, true, false, false, false));
#endif
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

#if BUILDFLAG(IS_WIN)  // These keys are Windows-only.
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, BackForwardKeys) {
  // Navigate to create some history.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://about/")));

  std::u16string before_back;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &before_back));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate back.
  {
    content::TestNavigationObserver navigation_observer(web_contents, 1);
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_BROWSER_BACK, false, false, false, false));
    navigation_observer.Wait();

    std::u16string after_back;
    ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &after_back));

    EXPECT_NE(before_back, after_back);
  }

  // And then forward.
  {
    content::TestNavigationObserver navigation_observer(web_contents, 1);
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_BROWSER_FORWARD, false, false, false, false));
    navigation_observer.Wait();

    std::u16string after_forward;
    ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &after_forward));

    EXPECT_EQ(before_back, after_forward);
  }
}
#endif

}  // namespace
