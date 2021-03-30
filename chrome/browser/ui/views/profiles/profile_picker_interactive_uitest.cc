// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Waits until the widget bounds change.
class WidgetBoundsChangeWaiter : public views::WidgetObserver {
 public:
  explicit WidgetBoundsChangeWaiter(views::Widget* widget) {
    DCHECK(widget);
    observation_.Observe(widget);
  }

  // Waits until the widget bounds change.
  void Wait() { run_loop_.Run(); }

 private:
  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

}  // namespace

class ProfilePickerInteractiveUiTest : public ProfilePickerTestBase {
 public:
  ProfilePickerInteractiveUiTest() = default;
  ~ProfilePickerInteractiveUiTest() override = default;

  void SendCloseWindowKeyboardCommand() {
    // Close window using keyboard.
#if defined(OS_MAC)
    // Use Cmd-W on Mac.
    bool control = false;
    bool shift = false;
    bool command = true;
    // Mac needs the widget to get focused (once again) for
    // SendKeyPressToWindowSync to work. A test-only particularity, pressing the
    // keybinding manually right in the run of the test actually replaces the
    // need of this call.
    ASSERT_TRUE(
        ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
#else
    // Use Ctrl-Shift-W on other platforms.
    bool control = true;
    bool shift = true;
    bool command = false;
#endif
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        widget()->GetNativeWindow(), ui::VKEY_W, control, shift, /*alt=*/false,
        command));
  }

  void SendBackKeyboardCommand() {
    // Close window using keyboard.
#if defined(OS_MAC)
    // Use Cmd-[ on Mac.
    bool alt = false;
    bool command = true;
    ui::KeyboardCode key = ui::VKEY_OEM_4;
    // Mac needs the widget to get focused (once again) for
    // SendKeyPressToWindowSync to work. A test-only particularity, pressing the
    // keybinding manually right in the run of the test actually replaces the
    // need of this call.
    ASSERT_TRUE(
        ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
#else
    // Use Ctrl-left on other platforms.
    bool alt = true;
    bool command = false;
    ui::KeyboardCode key = ui::VKEY_LEFT;
#endif
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        widget()->GetNativeWindow(), key, /*control=*/false,
        /*shift=*/false, alt, command));
  }
};

// Checks that the main picker view can be closed with keyboard shortcut.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, CloseWithKeyboard) {
  // Open a new picker.
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  WaitForLayoutWithoutToolbar();
  WaitForFirstPaint(web_contents(), GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
  // Closing the picker does not exit Chrome.
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
}

#if defined(OS_MAC)
// Checks that Chrome be closed with keyboard shortcut. Only MacOS has a
// keyboard shortcut to exit Chrome.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, ExitWithKeyboard) {
  // Open a new picker.
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  WaitForLayoutWithoutToolbar();
  WaitForFirstPaint(web_contents(), GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  content::WindowedNotificationObserver terminate_observer(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  // Send Cmd-Q.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      widget()->GetNativeWindow(), ui::VKEY_Q, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/true));
  // Check that Chrome is quitting.
  terminate_observer.Wait();
  WaitForPickerClosed();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}
#endif

// Checks that the main picker view can switch to full screen.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, FullscreenWithKeyboard) {
  // Open a new picker.
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  WaitForLayoutWithoutToolbar();
  WaitForFirstPaint(web_contents(), GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  EXPECT_FALSE(widget()->IsFullscreen());
  WidgetBoundsChangeWaiter bounds_waiter(widget());

  // Toggle fullscreen with keyboard.
#if defined(OS_MAC)
  // Use Cmd-Ctrl-F on Mac.
  bool control = true;
  bool command = true;
  ui::KeyboardCode key_code = ui::VKEY_F;
#else
  // Use F11 on other platforms.
  bool control = false;
  bool command = false;
  ui::KeyboardCode key_code = ui::VKEY_F11;
#endif
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      widget()->GetNativeWindow(), key_code, control, /*shift=*/false,
      /*alt=*/false, command));
  // Fullscreen causes the bounds of the widget to change.
  bounds_waiter.Wait();
  EXPECT_TRUE(widget()->IsFullscreen());
}

// Checks that the signin web view is able to process keyboard events.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       CloseSigninWithKeyboard) {
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile);
  WaitForLayoutWithoutToolbar();

  // Simulate a click on the signin button.
  base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
  EXPECT_CALL(switch_finished_callback, Run(true));
  ProfilePicker::SwitchToSignIn(SK_ColorRED, switch_finished_callback.Get());

  // Switch to the signin webview.
  WaitForLayoutWithToolbar();
  WaitForFirstPaint(web_contents(),
                    GaiaUrls::GetInstance()->signin_chrome_sync_dice());

  // Close the picker with the keyboard.
  EXPECT_TRUE(ProfilePicker::IsOpen());
  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
}

// Checks that both the signin web view and the main picker view are able to
// process a back keyboard event.
// Flaky on all platforms. http://crbug.com/1173544
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       DISABLED_NavigateBackWithKeyboard) {
  // Simulate walking through the flow starting at the picker so that navigating
  // back to the picker makes sense.
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  WaitForLayoutWithoutToolbar();
  WaitForFirstPaint(web_contents(), GURL("chrome://profile-picker"));
  web_contents()->GetController().LoadURL(
      GURL("chrome://profile-picker/new-profile"), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  WaitForFirstPaint(web_contents(),
                    GURL("chrome://profile-picker/new-profile"));

  // Simulate a click on the signin button.
  base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
  EXPECT_CALL(switch_finished_callback, Run(true));
  ProfilePicker::SwitchToSignIn(SK_ColorRED, switch_finished_callback.Get());

  // Switch to the signin webview.
  WaitForLayoutWithToolbar();
  WaitForFirstPaint(web_contents(),
                    GaiaUrls::GetInstance()->signin_chrome_sync_dice());

  // Navigate back with the keyboard.
  SendBackKeyboardCommand();

  WaitForLayoutWithoutToolbar();
  WaitForFirstPaint(web_contents(),
                    GURL("chrome://profile-picker/new-profile"));

  // Navigate again back with the keyboard.
  SendBackKeyboardCommand();
  WaitForFirstPaint(web_contents(), GURL("chrome://profile-picker"));

  // Navigating back once again does nothing.
  SendBackKeyboardCommand();
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}
