// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>

#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/ax_event_counter.h"

using base::StartsWith;

namespace {

// UI variations of the password save/update bubble to test.
enum PasswordBubbleTestFeature : uint32_t {
  // Standard 2-button dialog (Save/Update and Cancel).
  kNone = 0,
  // 3-button dialog variant featuring an explicit "Never" button.
  kThreeButtonSaveDialog = 1,
  // Split-button variant replacing Cancel with a dropdown menu offering
  // "Never".
  kDropdownMenuExperiment = 2,
};

std::string GetPasswordBubbleBrowserTestName(
    const testing::TestParamInfo<
        std::tuple<SyncConfiguration, bool, PasswordBubbleTestFeature>>& info) {
  const auto& [sync_config, is_rtl, experiment_feature] = info.param;
  std::string name;
  switch (sync_config) {
    case SyncConfiguration::kNotSyncing:
      name += "NotSyncing";
      break;
    case SyncConfiguration::kSyncing:
      name += "Syncing";
      break;
    case SyncConfiguration::kAccountStorageOnly:
      name += "AccountStorageOnly";
      break;
  }

  name += is_rtl ? "_RTL" : "_LTR";

  switch (experiment_feature) {
    case kNone:
      name += "_Default";
      break;
    case kThreeButtonSaveDialog:
      name += "_ThreeButtonSaveDialog";
      break;
    case kDropdownMenuExperiment:
      name += "_DropdownMenuExperiment";
      break;
  }
  return name;
}

}  // namespace

// Test params:
//  - SyncConfiguration : the sync state of the profile.
//  - bool : when true, the test is setup for RTL interfaces.
//  - PasswordBubbleTestFeature : the UI feature variation tested (standard
//    2-button dialog, 3-button dialog with "Never", or split-button dropdown).
class PasswordBubbleBrowserTest
    : public SupportsTestDialog<ManagePasswordsTest>,
      public testing::WithParamInterface<
          std::tuple<SyncConfiguration, bool, PasswordBubbleTestFeature>> {
 public:
  PasswordBubbleBrowserTest() {
    PasswordBubbleTestFeature experiment_feature = std::get<2>(GetParam());
    switch (experiment_feature) {
      case kNone:
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                features::kThreeButtonPasswordSaveDialog,
                features::kPasswordSaveUpdateDropdownMenuExperiment});
        break;
      case kThreeButtonSaveDialog:
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kThreeButtonPasswordSaveDialog},
            /*disabled_features=*/{
                features::kPasswordSaveUpdateDropdownMenuExperiment});
        break;
      case kDropdownMenuExperiment:
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/
            {features::kPasswordSaveUpdateDropdownMenuExperiment},
            /*disabled_features=*/{features::kThreeButtonPasswordSaveDialog});
        break;
    }
  }
  ~PasswordBubbleBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    const auto& [sync_config, is_rtl, experiment_feature] = GetParam();
    ConfigurePasswordSync(sync_config);
    scoped_rtl_.emplace(is_rtl);
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
          browser()->GetTabStripModel()->GetActiveWebContents())
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
  std::optional<base::i18n::ScopedRTLForTesting> scoped_rtl_;
};

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_PendingPasswordBubble) {
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
  SyncConfiguration sync_config = std::get<0>(GetParam());
  if (sync_config != SyncConfiguration::kAccountStorageOnly) {
    return;
  }
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
                     testing::Bool(),
                     testing::Values(kNone,
                                     kThreeButtonSaveDialog,
                                     kDropdownMenuExperiment)),
    GetPasswordBubbleBrowserTestName);

class PasswordAutoSignInToastTest : public base::test::WithFeatureOverride,
                                    public ManagePasswordsTest {
 public:
  PasswordAutoSignInToastTest()
      : base::test::WithFeatureOverride(
            password_manager::features::kCredentialManagementUnifiedUi) {}

  ToastController* GetToastController() {
    return ToastController::From(browser());
  }

  page_actions::PageActionTestAccessor GetIconAccessor() {
    return page_actions::PageActionTestAccessor(
        browser(), kActionShowPasswordsBubbleOrPage);
  }

  void WaitForIconVisibility(bool visible) {
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return GetIconAccessor().GetVisible() == visible; }));
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

  if (IsParamFeatureEnabled()) {
    // With Unified UI enabled, the icon should be HIDDEN while toast is shown.
    EXPECT_FALSE(GetIconAccessor().GetVisible());

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
    WaitForIconVisibility(true);
  } else {
    // With Legacy UI, the icon should be VISIBLE.
    EXPECT_TRUE(GetIconAccessor().GetVisible());
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

  // Icon should be hidden initially when toast is shown.
  EXPECT_FALSE(GetIconAccessor().GetVisible());

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

  browser()->GetTabStripModel()->ActivateTabAt(1);
  if (GetToastController()->GetToastCloseTimerForTesting()->IsRunning()) {
    GetToastController()->GetToastCloseTimerForTesting()->FireNow();
    EXPECT_TRUE(toast_destroyed.Wait());
  }

  // The icon should still be hidden, as the tab is not visible.
  EXPECT_FALSE(GetIconAccessor().GetVisible());

  // Switch back to Tab 0.
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // Verify Icon is now VISIBLE on Tab 0.
  WaitForIconVisibility(true);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PasswordAutoSignInToastTest);
