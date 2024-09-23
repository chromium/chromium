// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class ProfileBubbleInteractiveUiTest : public InProcessBrowserTest {
 public:
  // Returns the avatar button, which is the anchor view for the bubbles.
  views::View* GetAvatarButton() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    DCHECK(avatar_button);
    return avatar_button;
  }

  // Returns dummy parameters for the interception bubble.
  WebSigninInterceptor::Delegate::BubbleParameters GetTestBubbleParameters() {
    AccountInfo account;
    account.account_id = CoreAccountId::FromGaiaId("ID1");
    AccountInfo primary_account;
    primary_account.account_id = CoreAccountId::FromGaiaId("ID2");
    return WebSigninInterceptor::Delegate::BubbleParameters(
        WebSigninInterceptor::SigninInterceptionType::kMultiUser, account,
        primary_account);
  }

  // Waits until the bubble is displayed and focused, and returns the view that
  // was focused.
  views::View* WaitForFocus(views::DialogDelegate* bubble) {
    views::Widget* widget = bubble->GetWidget();
    EXPECT_TRUE(widget);
    views::test::WidgetVisibleWaiter(widget).Wait();
    views::View* focusable_view = bubble->GetInitiallyFocusedView();
    EXPECT_TRUE(focusable_view);
    ui_test_utils::WaitForViewFocus(browser(), focusable_view,
                                    /*focused=*/true);
    return focusable_view;
  }

  // Clears the focus on the current bubble.
  void ClearFocus(views::View* view) {
    EXPECT_TRUE(view->HasFocus());
    view->GetFocusManager()->ClearFocus();
    EXPECT_FALSE(view->HasFocus());
  }

  // Bring the focus back using the accessibility shortcut.
  void Refocus(views::View* view) {
    EXPECT_FALSE(view->HasFocus());
    // Mac uses Cmd-Option-ArrowDown, other platforms use F6.
#if BUILDFLAG(IS_MAC)
    ui::KeyboardCode key = ui::VKEY_DOWN;
    bool alt = true;
    bool command = true;
#else
    ui::KeyboardCode key = ui::VKEY_F6;
    bool alt = false;
    bool command = false;
#endif
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        browser()->window()->GetNativeWindow(), key, /*control=*/false,
        /*shift=*/false, alt, command));
    ui_test_utils::WaitForViewFocus(browser(), view, /*focused=*/true);
    EXPECT_TRUE(view->HasFocus());
  }
};

IN_PROC_BROWSER_TEST_F(ProfileBubbleInteractiveUiTest,
                       CustomizationBubbleFocus) {
  // Create the customization bubble, owned by the view hierarchy.
  ProfileCustomizationBubbleView* bubble = new ProfileCustomizationBubbleView(
      browser()->profile(), GetAvatarButton());
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  // The bubble takes focus when it is displayed.
  views::View* focused_view = WaitForFocus(bubble);
  ClearFocus(focused_view);
  // The bubble can be refocused using keyboard shortcuts.
  Refocus(focused_view);
  // Cleanup.
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

IN_PROC_BROWSER_TEST_F(ProfileBubbleInteractiveUiTest,
                       InterceptionBubbleFocus) {
  // Create the inteerception bubble, owned by the view hierarchy.
  DiceWebSigninInterceptionBubbleView* bubble =
      new DiceWebSigninInterceptionBubbleView(browser(), GetAvatarButton(),
                                              GetTestBubbleParameters(),
                                              base::DoNothing());
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  // The bubble takes focus when it is displayed.
  views::View* focused_view = WaitForFocus(bubble);
  ClearFocus(focused_view);
  // The bubble can be refocused using keyboard shortcuts.
  Refocus(focused_view);
  // Cleanup.
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

class ProfileMenuInteractiveUiTest : public ProfileBubbleInteractiveUiTest {
 public:
  void SetUp() override {
    // On linux, ui_test_utils::SendKeyPressSync() triggers the accelerator
    // twice. Again only on linux, the second keypress would close the bubble.
    // This flag avoids it for the test.
    ProfileMenuView::close_on_deactivate_for_testing_ = false;
    ProfileBubbleInteractiveUiTest::SetUp();
  }

  ProfileMenuViewBase* profile_menu_view() {
    auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
    return coordinator ? coordinator->GetProfileMenuViewBaseForTesting()
                       : nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(ProfileMenuInteractiveUiTest, OtherProfileFocus) {
  base::HistogramTester histogram_tester;
  // Add an additional profiles.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());

  // Open the menu using the keyboard.
  bool control = false;
  bool command = false;
#if BUILDFLAG(IS_MAC)
  command = true;
#else
  control = true;
#endif
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_M, control, /*shift=*/true, /*alt=*/false, command));
  ASSERT_TRUE(profile_menu_view());

  // This test doesn't care about performing the actual menu actions, only
  // about the histogram recorded.
  profile_menu_view()->set_perform_menu_actions_for_testing(false);

  // The first other profile menu should be focused when the menu is opened
  // via a key event.
  views::View* focused_view =
      profile_menu_view()->GetFocusManager()->GetFocusedView();
  ASSERT_TRUE(focused_view);
  focused_view->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE));
  focused_view->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_RETURN, ui::EF_NONE));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Profile.Menu.ClickedActionableItem",
      ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
      /*expected_bucket_count=*/1);
}
