// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This functionality currently works on Windows and on Linux when
// toolkit_views is defined (i.e. for Chrome OS). It's not needed
// on the Mac, and it's not yet implemented on Linux.

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// An async version of SendKeyPressSync since we don't get notified when a
// menu is showing.
void SendKeyPress(Browser* browser, ui::KeyboardCode key) {
  ASSERT_TRUE(ui_controls::SendKeyPress(
      browser->window()->GetNativeWindow(), key, false, false, false, false));
}

// Helper class that waits until the focus has changed to a view other
// than the one with the provided view id.
class ViewFocusChangeWaiter : public views::FocusChangeListener {
 public:
  ViewFocusChangeWaiter(views::FocusManager* focus_manager,
                        int previous_view_id)
      : focus_manager_(focus_manager),
        previous_view_id_(previous_view_id),
        weak_factory_(this) {
    focus_manager_->AddFocusChangeListener(this);
    // Call the focus change notification once in case the focus has
    // already changed.
    OnWillChangeFocus(NULL, focus_manager_->GetFocusedView());
  }

  ~ViewFocusChangeWaiter() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void Wait() {
    content::RunMessageLoop();
  }

 private:
  // Inherited from FocusChangeListener
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (focused_now && focused_now->id() != previous_view_id_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
    }
  }

  views::FocusManager* focus_manager_;
  int previous_view_id_;
  base::WeakPtrFactory<ViewFocusChangeWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewFocusChangeWaiter);
};

class SendKeysMenuListener : public views::MenuListener {
 public:
  SendKeysMenuListener(AppMenuButton* app_menu_button,
                       Browser* browser,
                       bool test_dismiss_menu)
      : app_menu_button_(app_menu_button),
        browser_(browser),
        menu_open_count_(0),
        test_dismiss_menu_(test_dismiss_menu) {
    app_menu_button_->AddMenuListener(this);
  }

  ~SendKeysMenuListener() override {
    if (test_dismiss_menu_)
      app_menu_button_->RemoveMenuListener(this);
  }

  int menu_open_count() const {
    return menu_open_count_;
  }

 private:
  // Overridden from views::MenuListener:
  void OnMenuOpened() override {
    menu_open_count_++;
    if (!test_dismiss_menu_) {
      app_menu_button_->RemoveMenuListener(this);
      // Press DOWN to select the first item, then RETURN to select it.
      SendKeyPress(browser_, ui::VKEY_DOWN);
      SendKeyPress(browser_, ui::VKEY_RETURN);
    } else {
      SendKeyPress(browser_, ui::VKEY_ESCAPE);
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
          base::TimeDelta::FromMilliseconds(200));
    }
  }

  AppMenuButton* app_menu_button_;
  Browser* browser_;
  // Keeps track of the number of times the menu was opened.
  int menu_open_count_;
  // If this is set then on receiving a notification that the menu was opened
  // we dismiss it by sending the ESC key.
  bool test_dismiss_menu_;

  DISALLOW_COPY_AND_ASSIGN(SendKeysMenuListener);
};

class KeyboardAccessTest : public InProcessBrowserTest {
 public:
  KeyboardAccessTest() {}

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
    return focused_view ? focused_view->id() : -1;
  }

  void WaitForFocusedViewIDToChange(int original_view_id) {
    if (GetFocusedViewID() != original_view_id)
      return;
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    views::FocusManager* focus_manager = widget->GetFocusManager();
    ViewFocusChangeWaiter waiter(focus_manager, original_view_id);
    waiter.Wait();
  }

#if defined(OS_WIN)
  // Opens the system menu on Windows with the Alt Space combination and selects
  // the New Tab option from the menu.
  void TestSystemMenuWithKeyboard();
#endif

  // Uses the keyboard to select the app menu i.e. with the F10 key.
  // It verifies that the menu when dismissed by sending the ESC key it does
  // not display twice.
  void TestMenuKeyboardAccessAndDismiss();

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardAccessTest);
};

void KeyboardAccessTest::TestMenuKeyboardAccess(bool alternate_key_sequence,
                                                bool shift,
                                                bool focus_omnibox) {
  // Navigate to a page in the first tab, which makes sure that focus is
  // set to the browser window.
  ui_test_utils::NavigateToURL(browser(), GURL("about:"));

  // The initial tab index should be 0.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Get the focused view ID, then press a key to activate the
  // page menu, then wait until the focused view changes.
  int original_view_id = GetFocusedViewID();

  content::WindowedNotificationObserver new_tab_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));

  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  SendKeysMenuListener menu_listener(browser_view->toolbar()->app_menu_button(),
                                     browser(), false);

  if (focus_omnibox)
    browser()->window()->GetLocationBar()->FocusLocation(false);

#if defined(OS_CHROMEOS)
  // Chrome OS doesn't have a way to just focus the app menu, so we use Alt+F to
  // bring up the menu.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, shift, true, false));
#else
  ui::KeyboardCode menu_key =
      alternate_key_sequence ? ui::VKEY_MENU : ui::VKEY_F10;
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), menu_key, false, shift, false, false));
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
#if !defined(OS_CHROMEOS)
  if (alternate_key_sequence)
    SendKeyPress(browser(), ui::VKEY_DOWN);
  else
    SendKeyPress(browser(), ui::VKEY_RETURN);
#endif

  // Wait for the new tab to appear.
  new_tab_observer.Wait();

  // Make sure that the new tab index is 1.
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
}

#if defined(OS_WIN)

// This CBT hook is set for the duration of the TestSystemMenuWithKeyboard test
LRESULT CALLBACK SystemMenuTestCBTHook(int n_code,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  // Look for the system menu window getting created or becoming visible and
  // then select the New Tab option from the menu.
  if (n_code == HCBT_ACTIVATE || n_code == HCBT_CREATEWND) {
    wchar_t class_name[MAX_PATH] = {0};
    GetClassName(reinterpret_cast<HWND>(w_param),
                 class_name,
                 arraysize(class_name));
    if (base::LowerCaseEqualsASCII(class_name, "#32768")) {
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
  ui_test_utils::NavigateToURL(browser(), GURL("about:"));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  content::WindowedNotificationObserver new_tab_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));
  // Sending the Alt space keys to the browser will bring up the system menu
  // which runs a model loop. We set a CBT hook to look for the menu and send
  // keystrokes to it.
  HHOOK cbt_hook = ::SetWindowsHookEx(WH_CBT,
                                      SystemMenuTestCBTHook,
                                      NULL,
                                      ::GetCurrentThreadId());
  ASSERT_TRUE(cbt_hook != NULL);

  bool ret = ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_SPACE, false, false, true, false);
  EXPECT_TRUE(ret);

  if (ret) {
    // Wait for the new tab to appear.
    new_tab_observer.Wait();
    // Make sure that the new tab index is 1.
    ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  }
  ::UnhookWindowsHookEx(cbt_hook);
}
#endif

void KeyboardAccessTest::TestMenuKeyboardAccessAndDismiss() {
  ui_test_utils::NavigateToURL(browser(), GURL("about:"));

  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  int original_view_id = GetFocusedViewID();

  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  SendKeysMenuListener menu_listener(browser_view->toolbar()->app_menu_button(),
                                     browser(), true);

  browser()->window()->GetLocationBar()->FocusLocation(false);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F10, false, false, false, false));

  WaitForFocusedViewIDToChange(original_view_id);

  SendKeyPress(browser(), ui::VKEY_DOWN);
  content::RunMessageLoop();
  ASSERT_EQ(1, menu_listener.menu_open_count());
}

// http://crbug.com/62310.
#if defined(OS_CHROMEOS)
#define MAYBE_TestMenuKeyboardAccess DISABLED_TestMenuKeyboardAccess
#elif defined(OS_MACOSX)
// No keyboard shortcut for the Chrome menu on Mac: http://crbug.com/823952
#define MAYBE_TestMenuKeyboardAccess DISABLED_TestMenuKeyboardAccess
#else
#define MAYBE_TestMenuKeyboardAccess TestMenuKeyboardAccess
#endif

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_TestMenuKeyboardAccess) {
  TestMenuKeyboardAccess(false, false, false);
}

// http://crbug.com/62310.
#if defined(OS_CHROMEOS)
#define MAYBE_TestAltMenuKeyboardAccess DISABLED_TestAltMenuKeyboardAccess
#elif defined(OS_MACOSX)
// No keyboard shortcut for the Chrome menu on Mac: http://crbug.com/823952
#define MAYBE_TestAltMenuKeyboardAccess DISABLED_TestAltMenuKeyboardAccess
#else
#define MAYBE_TestAltMenuKeyboardAccess TestAltMenuKeyboardAccess
#endif
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_TestAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, false, false);
}

// If this flakes, use http://crbug.com/62311.
#if defined(OS_WIN)
#define MAYBE_TestShiftAltMenuKeyboardAccess DISABLED_TestShiftAltMenuKeyboardAccess
#else
#define MAYBE_TestShiftAltMenuKeyboardAccess TestShiftAltMenuKeyboardAccess
#endif
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest,
                       MAYBE_TestShiftAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, true, false);
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest,
                       DISABLED_TestAltMenuKeyboardAccessFocusOmnibox) {
  TestMenuKeyboardAccess(true, false, true);
}

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, TestSystemMenuWithKeyboard) {
  TestSystemMenuWithKeyboard();
}
#endif

#if !defined(OS_WIN) && defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, TestMenuKeyboardOpenDismiss) {
  TestMenuKeyboardAccessAndDismiss();
}
#endif

#if defined(OS_MACOSX)
// Focusing or input is not completely working on Mac: http://crbug.com/824418
#define MAYBE_ReserveKeyboardAccelerators DISABLED_ReserveKeyboardAccelerators
#else
#define MAYBE_ReserveKeyboardAccelerators ReserveKeyboardAccelerators
#endif
// Test that JavaScript cannot intercept reserved keyboard accelerators like
// ctrl-t to open a new tab or ctrl-f4 to close a tab.
// TODO(isherman): This test times out on ChromeOS.  We should merge it with
// BrowserKeyEventsTest.ReservedAccelerators, but just disable for now.
// If this flakes, use http://crbug.com/62311.
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_ReserveKeyboardAccelerators) {
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_TAB, true, false, false, false));
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_W, true, false, false, false));
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

#if defined(OS_WIN)  // These keys are Windows-only.
// Disabled on debug due to high flake rate; see https://crbug.com/846623.
#if !defined(NDEBUG)
#define MAYBE_BackForwardKeys DISABLED_BackForwardKeys
#else
#define MAYBE_BackForwardKeys BackForwardKeys
#endif
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_BackForwardKeys) {
  // Navigate to create some history.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://about/"));

  base::string16 before_back;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &before_back));

  // Navigate back.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_BROWSER_BACK, false, false, false, false));

  base::string16 after_back;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &after_back));

  EXPECT_NE(before_back, after_back);

  // And then forward.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_BROWSER_FORWARD, false, false, false, false));

  base::string16 after_forward;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &after_forward));

  EXPECT_EQ(before_back, after_forward);
}
#endif

}  // namespace
