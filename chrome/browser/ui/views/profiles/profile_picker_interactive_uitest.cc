// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
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

  void ShowAndFocusPicker(ProfilePicker::EntryPoint entry_point,
                          const GURL& expected_url) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(entry_point));
    WaitForLoadStop(expected_url);
    EXPECT_TRUE(
        ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
  }

  void SendCloseWindowKeyboardCommand() {
    // Close window using keyboard.
#if BUILDFLAG(IS_MAC)
    // Use Cmd-W on Mac.
    bool control = false;
    bool shift = false;
    bool command = true;
#else
    // Use Ctrl-Shift-W on other platforms.
    bool control = true;
    bool shift = true;
    bool command = false;
#endif
    SendKeyPress(ui::VKEY_W, control, shift, /*alt=*/false, command);
  }

  void SendBackKeyboardCommand() {
    // Close window using keyboard.
#if BUILDFLAG(IS_MAC)
    // Use Cmd-[ on Mac.
    bool alt = false;
    bool command = true;
    ui::KeyboardCode key = ui::VKEY_OEM_4;
#else
    // Use Ctrl-left on other platforms.
    bool alt = true;
    bool command = false;
    ui::KeyboardCode key = ui::VKEY_LEFT;
#endif
    SendKeyPress(key, /*control=*/false, /*shift=*/false, alt, command);
  }

  void SendKeyPress(ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) {
#if BUILDFLAG(IS_MAC)
    // Mac needs the widget to get focused (once again) for
    // SendKeyPressToWindowSync to work. A test-only particularity, pressing the
    // keybinding manually right in the run of the test actually replaces the
    // need of this call.
    ASSERT_TRUE(
        ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
#endif
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        widget()->GetNativeWindow(), key, control, shift, alt, command));
  }

  void SimulateUserActivation() {
    content::UpdateUserActivationStateInterceptor user_activation_interceptor(
        web_contents()->GetPrimaryMainFrame());
    user_activation_interceptor.UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
  }
};

// Checks that the main picker view can be closed with keyboard shortcut.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, CloseWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
  // Closing the picker does not exit Chrome.
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
}

#if BUILDFLAG(IS_MAC)
// Checks that Chrome be closed with keyboard shortcut. Only MacOS has a
// keyboard shortcut to exit Chrome.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, ExitWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Send Cmd-Q.
  SendKeyPress(ui::VKEY_Q, /*control=*/false, /*shift=*/false, /*alt=*/false,
               /*command=*/true);
  WaitForPickerClosed();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}
#endif

// Checks that the main picker view can switch to full screen.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, FullscreenWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_FALSE(widget()->IsFullscreen());
  WidgetBoundsChangeWaiter bounds_waiter(widget());

  // Toggle fullscreen with keyboard.
#if BUILDFLAG(IS_MAC)
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
  SendKeyPress(key_code, control, /*shift=*/false, /*alt=*/false, command);
  // Fullscreen causes the bounds of the widget to change.
  bounds_waiter.Wait();
  EXPECT_TRUE(widget()->IsFullscreen());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Checks that the signin web view is able to process keyboard events.
// TODO(https://crbug.com/1227800): Flaky on linux and Windows.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_CloseDiceSigninWithKeyboard DISABLED_CloseDiceSigninWithKeyboard
#else
#define MAYBE_CloseDiceSigninWithKeyboard CloseDiceSigninWithKeyboard
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       MAYBE_CloseDiceSigninWithKeyboard) {
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile,
                     GURL("chrome://profile-picker/new-profile"));

  // Simulate a click on the signin button.
  base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
  EXPECT_CALL(switch_finished_callback, Run(true));
  ProfilePicker::SwitchToDiceSignIn(SK_ColorRED,
                                    switch_finished_callback.Get());

  // Switch to the signin webview.
  WaitForLoadStop(signin::GetChromeSyncURLForDice({.for_promo_flow = true}));

  // Close the picker with the keyboard.
  EXPECT_TRUE(ProfilePicker::IsOpen());
  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
}

// Checks that both the signin web view and the main picker view are able to
// process a back keyboard event.
// TODO(https://crbug.com/1173544): Flaky on linux, Win7, Mac
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_NavigateBackFromDiceSigninWithKeyboard \
  DISABLED_NavigateBackFromDiceSigninWithKeyboard
#else
#define MAYBE_NavigateBackFromDiceSigninWithKeyboard \
  NavigateBackFromDiceSigninWithKeyboard
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       MAYBE_NavigateBackFromDiceSigninWithKeyboard) {
  // Simulate walking through the flow starting at the picker so that navigating
  // back to the picker makes sense. Check that the navigation list is populated
  // correctly.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_EQ(1, web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(0, web_contents()->GetController().GetLastCommittedEntryIndex());
  web_contents()->GetController().LoadURL(
      GURL("chrome://profile-picker/new-profile"), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));
  EXPECT_EQ(2, web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(1, web_contents()->GetController().GetLastCommittedEntryIndex());

  // Simulate a click on the signin button.
  base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
  EXPECT_CALL(switch_finished_callback, Run(true));
  ProfilePicker::SwitchToDiceSignIn(SK_ColorRED,
                                    switch_finished_callback.Get());

  // Switch to the signin webview.
  WaitForLoadStop(signin::GetChromeSyncURLForDice({.for_promo_flow = true}));

  // Navigate back with the keyboard.
  SendBackKeyboardCommand();
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));
  EXPECT_EQ(1, web_contents()->GetController().GetLastCommittedEntryIndex());

  // Navigate again back with the keyboard.
  SendBackKeyboardCommand();
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_EQ(0, web_contents()->GetController().GetLastCommittedEntryIndex());

  // Navigating back once again does nothing.
  SendBackKeyboardCommand();
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_NavigateBackFromProfileTypeChoiceWithKeyboard \
  DISABLED_NavigateBackFromProfileTypeChoiceWithKeyboard
#else
#define MAYBE_NavigateBackFromProfileTypeChoiceWithKeyboard \
  NavigateBackFromProfileTypeChoiceWithKeyboard
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       MAYBE_NavigateBackFromProfileTypeChoiceWithKeyboard) {
  // Simulate walking through the flow starting at the picker so that navigating
  // back to the picker makes sense. Check that the navigation list is populated
  // correctly.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_EQ(1, web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(0, web_contents()->GetController().GetLastCommittedEntryIndex());
  web_contents()->GetController().LoadURL(
      GURL("chrome://profile-picker/new-profile"), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));
  EXPECT_EQ(2, web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(1, web_contents()->GetController().GetLastCommittedEntryIndex());

  // Navigate back with the keyboard.
  SendBackKeyboardCommand();
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_EQ(0, web_contents()->GetController().GetLastCommittedEntryIndex());

  // Navigating back once again does nothing.
  SendBackKeyboardCommand();
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       NavigateBackFromNewProfileWithKeyboard) {
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile,
                     GURL("chrome://profile-picker/new-profile"));
  EXPECT_EQ(2, web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(1, web_contents()->GetController().GetLastCommittedEntryIndex());

  // For applying the history manipulation, it needs user activation.
  SimulateUserActivation();
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());

  // Navigate back with the keyboard.
  SendBackKeyboardCommand();
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_EQ(0, web_contents()->GetController().GetLastCommittedEntryIndex());
}
