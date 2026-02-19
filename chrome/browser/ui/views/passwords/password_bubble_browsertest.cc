// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/ax_event_counter.h"

using base::StartsWith;

// Test params:
//  - bool : when true, the test is setup for users that sync their passwords.
//  - bool : when true, the test is setup for RTL interfaces.
class PasswordBubbleBrowserTest
    : public SupportsTestDialog<ManagePasswordsTest>,
      public testing::WithParamInterface<std::tuple<SyncConfiguration, bool>> {
 public:
  PasswordBubbleBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kThreeButtonPasswordSaveDialog, false);
  }
  ~PasswordBubbleBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    ConfigurePasswordSync(std::get<0>(GetParam()));
    base::i18n::SetRTLForTesting(std::get<1>(GetParam()));
    if (StartsWith(name, "PendingPasswordBubble",
                   base::CompareCase::SENSITIVE)) {
      SetupPendingPassword();
    } else if (StartsWith(name, "AutomaticPasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupAutomaticPassword();
    } else if (StartsWith(name, "ManagePasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      // Set test form to be account-stored. Otherwise, there is no indicator.
      test_form()->in_store =
          password_manager::PasswordForm::Store::kAccountStore;
      SetupManagingPasswords();
      ExecuteManagePasswordsCommand();
    } else if (StartsWith(name, "AutoSignin", base::CompareCase::SENSITIVE)) {
      test_form()->url = GURL("https://example.com");
      test_form()->display_name = u"test_user";
      test_form()->username_value = u"test_user@gmail.com";
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials;
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(*test_form()));

      PasswordAutoSignInView::set_auto_signin_toast_timeout(10);
      SetupAutoSignin(std::move(local_credentials));
    } else if (StartsWith(name, "MoveToAccountStoreBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupMovingPasswords();
    } else if (StartsWith(name, "AccountChooser",
                          base::CompareCase::SENSITIVE)) {
      test_form()->url = GURL("https://example.com");
      test_form()->display_name = u"test_user";
      test_form()->username_value = u"test_user@gmail.com";
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials;
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(*test_form()));

      ChromePasswordManagerClient::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->PromptUserToChooseCredentials(std::move(local_credentials),
                                          url::Origin::Create(test_form()->url),
                                          base::DoNothing());
    } else if (StartsWith(name, "SafeState", base::CompareCase::SENSITIVE)) {
      SetupSafeState();
    } else if (StartsWith(name, "MoreToFixState",
                          base::CompareCase::SENSITIVE)) {
      SetupMoreToFixState();
    } else {
      ADD_FAILURE() << "Unknown dialog type";
      return;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_PendingPasswordBubble) {
  set_baseline("7267604");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_AutomaticPasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_ManagePasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_AutoSignin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_AccountChooser) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_SafeState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_MoreToFixState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_MoveToAccountStoreBubble) {
  // This test is only relevant for account storage users.
  if (std::get<0>(GetParam()) != SyncConfiguration::kAccountStorageOnly) {
    return;
  }
  set_baseline("5855019");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  // This needs to show a password bubble that does not trigger as a user
  // gesture in order to fire an alert event. See
  // LocationBarBubbleDelegateView's calls to SetAccessibleWindowRole().
  ShowUi("AutomaticPasswordBubble");
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordBubbleBrowserTest,
    testing::Combine(testing::Values(SyncConfiguration::kNotSyncing,
                                     SyncConfiguration::kAccountStorageOnly,
                                     SyncConfiguration::kSyncing),
                     testing::Bool()));

// This subclass exists to exercise the 3-button save password dialog via its
// feature flag. When that feature launches, remove this and update the original
// case to be 3-button.
class ThreeButtonPasswordBubbleBrowserTest : public PasswordBubbleBrowserTest {
 public:
  ThreeButtonPasswordBubbleBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kThreeButtonPasswordSaveDialog, true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ThreeButtonPasswordBubbleBrowserTest,
                       InvokeUi_PendingPasswordBubble) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ThreeButtonPasswordBubbleBrowserTest,
    testing::Combine(testing::Values(SyncConfiguration::kNotSyncing),
                     testing::Bool()));

class PasswordAutoSignInToastTest : public base::test::WithFeatureOverride,
                                    public ManagePasswordsTest {
 public:
  PasswordAutoSignInToastTest()
      : base::test::WithFeatureOverride(
            password_manager::features::kCredentialManagementUnifiedUi) {}

  ToastController* GetToastController() {
    return browser()->browser_window_features()->toast_controller();
  }

  IconLabelBubbleView* GetIconView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionView(kActionShowPasswordsBubbleOrPage);
  }

  void WaitForIconVisibility(IconLabelBubbleView* icon, bool visible) {
    if (icon->GetVisible() == visible) {
      return;
    }
    base::test::TestFuture<void> future;
    auto subscription =
        icon->AddVisibleChangedCallback(future.GetRepeatingCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(icon->GetVisible(), visible);
  }
};

IN_PROC_BROWSER_TEST_P(PasswordAutoSignInToastTest, Shows) {
  test_form()->url = GURL("https://example.com");
  test_form()->display_name = u"test_user";
  test_form()->username_value = u"test_user@gmail.com";
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(*test_form()));

  SetupAutoSignin(std::move(local_credentials));

  if (IsParamFeatureEnabled()) {
    // Verify toast is showing.
    EXPECT_TRUE(GetToastController()->IsShowingToast());
    EXPECT_EQ(GetToastController()->GetCurrentToastId(), ToastId::kAutoSignIn);
  } else {
    // Verify bubble is showing.
    EXPECT_TRUE(PasswordBubbleViewBase::manage_password_bubble());
  }
}

IN_PROC_BROWSER_TEST_P(PasswordAutoSignInToastTest, CheckIconVisibility) {
  // Setup Auto Sign-in
  test_form()->url = GURL("https://example.com");
  test_form()->display_name = u"test_user";
  test_form()->username_value = u"test_user@gmail.com";
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(*test_form()));

  SetupAutoSignin(std::move(local_credentials));

  IconLabelBubbleView* icon = GetIconView();
  ASSERT_TRUE(icon);

  if (IsParamFeatureEnabled()) {
    // With Unified UI enabled, the icon should be HIDDEN while toast is shown.
    EXPECT_FALSE(icon->GetVisible());

    // Wait for the toast to be destroyed.
    base::test::TestFuture<void> toast_destroyed;
    auto subscription = GetToastController()->RegisterOnWidgetDestroyed(
        base::BindLambdaForTesting([&](ToastId toast_id) {
          if (toast_id == ToastId::kAutoSignIn) {
            toast_destroyed.SetValue();
          }
        }));
    GetToastController()->GetToastCloseTimerForTesting()->FireNow();
    EXPECT_TRUE(toast_destroyed.Wait());

    // The icon should reappear.
    WaitForIconVisibility(icon, true);
  } else {
    // With Legacy UI, the icon should be VISIBLE.
    EXPECT_TRUE(icon->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(PasswordAutoSignInToastTest, TabSwitch) {
  if (!IsParamFeatureEnabled()) {
    return;
  }

  // Setup Auto Sign-in on current tab (Tab 0)
  test_form()->url = GURL("https://example.com");
  test_form()->display_name = u"test_user";
  test_form()->username_value = u"test_user@gmail.com";
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(*test_form()));

  SetupAutoSignin(std::move(local_credentials));

  IconLabelBubbleView* icon = GetIconView();
  // Icon should be hidden initially when toast is shown.
  EXPECT_FALSE(icon->GetVisible());

  // Open a new tab (Tab 1) and switch to it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // Wait for the toast on tab 0 to be destroyed.
  base::test::TestFuture<void> toast_destroyed;
  auto subscription = GetToastController()->RegisterOnWidgetDestroyed(
      base::BindLambdaForTesting([&](ToastId toast_id) {
        if (toast_id == ToastId::kAutoSignIn) {
          toast_destroyed.SetValue();
        }
      }));

  browser()->tab_strip_model()->ActivateTabAt(1);
  if (GetToastController()->GetToastCloseTimerForTesting()->IsRunning()) {
    GetToastController()->GetToastCloseTimerForTesting()->FireNow();
    EXPECT_TRUE(toast_destroyed.Wait());
  }

  // The icon should still be hidden, as the tab is not visible.
  EXPECT_FALSE(icon->GetVisible());

  // Switch back to Tab 0.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify Icon is now VISIBLE on Tab 0.
  WaitForIconVisibility(icon, true);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PasswordAutoSignInToastTest);
