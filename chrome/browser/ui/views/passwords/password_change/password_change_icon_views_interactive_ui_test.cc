// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/run_until.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_icon_views.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/vector_icons.h"

using testing::Return;

class PasswordChangeIconViewsTest : public ManagePasswordsTest {
 public:
  PasswordChangeIconViewsTest() = default;

  PasswordChangeIconViewsTest(const PasswordChangeIconViewsTest&) = delete;
  PasswordChangeIconViewsTest& operator=(const PasswordChangeIconViewsTest&) =
      delete;

  ~PasswordChangeIconViewsTest() override = default;

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  PasswordChangeIconViews* GetView() {
    views::View* const view =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kChangePassword);
    CHECK(views::IsViewClass<PasswordChangeIconViews>(view));
    return static_cast<PasswordChangeIconViews*>(view);
  }

  void SetPrivacyNoticeAcceptedPref() {
    browser()->profile()->GetPrefs()->SetInteger(
        optimization_guide::prefs::GetSettingEnabledPrefName(
            optimization_guide::UserVisibleFeatureKey::
                kPasswordChangeSubmission),
        static_cast<int>(
            optimization_guide::prefs::FeatureOptInState::kEnabled));
  }

  void EnableSignIn() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    ManagePasswordsTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SetUpOnMainThread() override {
    ManagePasswordsTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(PasswordChangeIconViewsTest,
                       ViewIsNotVisibleWhenManagingPasswords) {
  SetupManagingPasswords();
  EXPECT_FALSE(GetView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeIconViewsTest,
    ViewIsVisibleWhenChangingPasswordWaitingForPrivacyNotice) {
  EnableSignIn();
  SetupPasswordChange();
  EXPECT_TRUE(GetView()->GetVisible());
  EXPECT_EQ(vector_icons::kPasswordManagerIcon.name,
            GetView()->GetVectorIcon().name);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeIconViewsTest,
    ViewIsVisibleWhenChangingPasswordWaitingForPasswordForm) {
  EnableSignIn();
  SetPrivacyNoticeAcceptedPref();
  SetupPasswordChange();
  EXPECT_TRUE(GetView()->GetVisible());
  EXPECT_EQ(views::kPasswordChangeIcon.name, GetView()->GetVectorIcon().name);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_SIGN_IN_CHECK),
            GetView()->GetText());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeIconViewsTest,
                       ViewIsVisibleWhenChangingPasswordFinished) {
  EnableSignIn();
  SetPrivacyNoticeAcceptedPref();
  SetupPasswordChange();
  // Wait until the password change flow runs. The flow will fail
  // because OptimizationGuideKeyedService is not set up for this test. But it
  // doesn't matter here, it only checks that the icon should change
  // back to standard password manger icon and label should be removed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    PasswordChangeDelegate* delegate =
        static_cast<PasswordsModelDelegate*>(GetController())
            ->GetPasswordChangeDelegate();
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));
  EXPECT_TRUE(GetView()->GetVisible());
  EXPECT_EQ(vector_icons::kPasswordManagerIcon.name,
            GetView()->GetVectorIcon().name);
  EXPECT_EQ(u"", GetView()->GetText());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeIconViewsTest,
                       ViewIsNotVisibleWhenChangingPasswordCanceled) {
  SetupPasswordChange();
  PasswordChangeDelegate* delegate =
      static_cast<PasswordsModelDelegate*>(GetController())
          ->GetPasswordChangeDelegate();
  delegate->Stop();
  EXPECT_FALSE(GetView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeIconViewsTest, TestPinnedToolbarTooltip) {
  PinnedToolbarActionsModel::Get(browser()->profile())
      ->UpdatePinnedState(kActionShowPasswordsBubbleOrPage, true);
  BrowserActions* browser_actions = browser()->browser_actions();
  std::u16string_view tooltip =
      actions::ActionManager::Get()
          .FindAction(kActionShowPasswordsBubbleOrPage,
                      browser_actions->root_action_item())
          ->GetTooltipText();
  ASSERT_EQ(tooltip,
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE));

  SetupPasswordChange();
  tooltip = actions::ActionManager::Get()
                .FindAction(kActionShowPasswordsBubbleOrPage,
                            browser_actions->root_action_item())
                ->GetTooltipText();
  ASSERT_EQ(tooltip, l10n_util::GetStringUTF16(
                         IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_ICON_TOOLTIP));
}
