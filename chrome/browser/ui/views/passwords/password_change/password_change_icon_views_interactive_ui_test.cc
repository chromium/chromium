// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_icon_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/vector_icons.h"

using testing::Return;

class PasswordChangeIconViewsTest : public ManagePasswordsTest {
 public:
 public:
  PasswordChangeIconViewsTest() = default;

  PasswordChangeIconViewsTest(const PasswordChangeIconViewsTest&) = delete;
  PasswordChangeIconViewsTest& operator=(const PasswordChangeIconViewsTest&) =
      delete;

  ~PasswordChangeIconViewsTest() override = default;

  PasswordChangeIconViews* GetView() {
    views::View* const view =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kChangePassword);
    CHECK(views::IsViewClass<PasswordChangeIconViews>(view));
    return static_cast<PasswordChangeIconViews*>(view);
  }

  void SetPrivacyNoticeAcceptedPref() {
    browser()->profile()->GetPrefs()->SetBoolean(
        password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);
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
  SetupPasswordChange();
  EXPECT_TRUE(GetView()->GetVisible());
  EXPECT_EQ(vector_icons::kPasswordManagerIcon.name,
            GetView()->GetVectorIcon().name);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeIconViewsTest,
    ViewIsVisibleWhenChangingPasswordWaitingForPasswordForm) {
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
