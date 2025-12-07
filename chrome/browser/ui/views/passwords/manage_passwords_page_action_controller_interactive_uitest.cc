// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_page_action_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

class ManagePasswordsControllerTest : public ManagePasswordsTest {
 public:
  ManagePasswordsControllerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationManagePasswords.name, "true"}});
  }

  ~ManagePasswordsControllerTest() override = default;

  password_manager::ui::State GetViewState() {
    return GetController()->GetState();
  }

  views::View* GetIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionView(kActionShowPasswordsBubbleOrPage);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       IconIsVisibleInManageState) {
  // Make sure the icon is not showing initially.
  ASSERT_TRUE(GetIcon());
  EXPECT_FALSE(GetIcon()->GetVisible());
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, GetViewState());
  // The icon should show in the new state.
  ASSERT_TRUE(GetIcon());
  EXPECT_TRUE(GetIcon()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest, AutoPopupAndIconState) {
  // Set up the controller to be in a pending password state, which should
  // trigger an automatic bubble pop-up.
  SetupPendingPassword();
  // Verify that the password bubble is now showing.
  EXPECT_TRUE(PasswordBubbleViewBase::manage_password_bubble());
  ASSERT_TRUE(GetIcon());
  EXPECT_TRUE(GetIcon()->GetVisible());
  // The tooltip should be empty because the bubble is showing.
  EXPECT_EQ(GetIcon()->GetTooltipText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       CredentialRequestDialogIconState) {
  // Start in a MANAGE_STATE with some credentials, so the icon is visible.
  SetupManagingPasswords();
  EXPECT_EQ(GetController()->GetState(), password_manager::ui::MANAGE_STATE);
  // Verify initial icon visibility and tooltip.
  ASSERT_TRUE(GetIcon());
  EXPECT_TRUE(GetIcon()->GetVisible());
  EXPECT_EQ(GetIcon()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE));
  // Simulate a credential request, which opens a modal dialog.
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(*test_form()));
  url::Origin origin = url::Origin::Create(test_form()->url);

  // Use a simple callback to allow the dialog creation to complete.
  base::RunLoop run_loop;
  ManagePasswordsState::CredentialsCallback callback = base::BindOnce(
      [](base::OnceClosure quit_closure,
         const password_manager::PasswordForm* form) {
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  // OnChooseCredentials creates and shows the credential request dialog, and
  // sets the controller's state to CREDENTIAL_REQUEST_STATE.
  EXPECT_TRUE(GetController()->OnChooseCredentials(
      std::move(local_credentials), origin, std::move(callback)));

  // The omnibox bubble should NOT be showing, as the credential request is a
  // separate modal dialog.
  EXPECT_FALSE(PasswordBubbleViewBase::manage_password_bubble());

  // The controller's state should have changed to CREDENTIAL_REQUEST_STATE,
  // which hides the icon and clears the tooltip.
  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::CREDENTIAL_REQUEST_STATE);
  EXPECT_FALSE(GetIcon()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       IconIsHiddenInDefaultInactiveState) {
  SetupManagingPasswords();
  // Make sure the icon is showing initially.
  ASSERT_TRUE(GetIcon());
  EXPECT_TRUE(GetIcon()->GetVisible());
  // Navigate to a new blank page. This action causes the
  // ManagePasswordsUIController for the tab to reset its state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  // The icon should not show in the new state.
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, GetViewState());
  ASSERT_TRUE(GetIcon());
  EXPECT_FALSE(GetIcon()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       BubbleIsAutomaticallyOpening) {
  password_manager::PasswordForm non_shared_credentials;
  non_shared_credentials.url = GURL("http://example.com/login");
  non_shared_credentials.signon_realm = non_shared_credentials.url.spec();
  non_shared_credentials.username_value = u"username";
  non_shared_credentials.password_value = u"12345";
  non_shared_credentials.match_type =
      password_manager::PasswordForm::MatchType::kExact;

  password_manager::PasswordForm shared_credentials = non_shared_credentials;
  shared_credentials.type =
      password_manager::PasswordForm::Type::kReceivedViaSharing;
  non_shared_credentials.username_value = u"username2";
  shared_credentials.sharing_notification_displayed = false;
  std::vector<password_manager::PasswordForm> forms = {shared_credentials,
                                                       non_shared_credentials};
  GetController()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  ASSERT_EQ(2u, GetController()->GetCurrentForms().size());
  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS);
  // With the current state of the controller a bubble should open
  // automatically.
  EXPECT_TRUE(GetController()->IsShowingBubble());
  // All interactions with the bubble will close it and invoke OnBubbleHidden().
  GetController()->OnBubbleHidden();
  EXPECT_EQ(GetController()->GetState(), password_manager::ui::MANAGE_STATE);
}
// This test verifies that a crash does not occur in a specific scenario
// involving tab navigation. The test sets up a pending password state, then
// simulates opening a new tab and navigating the original tab, and finally
// asserts that the Manage Passwords UI is in an inactive state at the end.
IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       ReproduceCrashOnPechewebNavigation) {
  SetupPendingPassword();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetViewState());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ui_test_utils::AllBrowserTabAddedWaiter tabs_waiter;
  content::TestNavigationObserver nav_observer(web_contents);

  ASSERT_TRUE(content::ExecJs(
      web_contents, "window.open(); window.location = 'http://example.com';"));

  nav_observer.Wait();
  tabs_waiter.Wait();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, GetViewState());
}
