// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

const char kTestPassphrase[] = "passphrase";

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7146804";

class AccountSettingsPagePixelBrowserTest : public InteractiveBrowserTest {
 public:
  AccountSettingsPagePixelBrowserTest() = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  void SigninWithFullInfo() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@gmail.com", signin::ConsentLevel::kSignin);
    ASSERT_FALSE(account_info.IsEmpty());

    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("John Testing")
                       .SetGivenName("John")
                       .SetAvatarUrl("PICTURE_URL")
                       .SetHostedDomain(std::string())
                       .SetLocale("en")
                       .Build();
    ASSERT_TRUE(account_info.IsValid());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    signin::SimulateAccountImageFetch(
        identity_manager, account_info.account_id, "SIGNED_IN_IMAGE_URL",
        gfx::test::CreateImage(20, 20, SK_ColorBLUE));
  }

  void DisableAllSyncDatatypes() {
    GetTestSyncService()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, syncer::UserSelectableTypeSet());
    GetTestSyncService()->FireStateChanged();
  }

  void SimulatePassphraseError() {
    GetTestSyncService()->GetUserSettings()->SetPassphraseRequired(
        std::string(kTestPassphrase));
    GetTestSyncService()->FireStateChanged();
  }

  void ClearPassphraseError() {
    GetTestSyncService()->GetUserSettings()->SetDecryptionPassphrase(
        std::string(kTestPassphrase));
    GetTestSyncService()->FireStateChanged();
  }

 protected:
  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

  // Helper to navigate to the account settings page and take a screenshot.
  auto NavigateToSettingsAndScreenshot(const std::string& screenshot_name,
                                       bool scroll_to_bottom = false) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
    const DeepQuery kAvatarRowQuery = {"settings-ui",
                                       "settings-main#main",
                                       "settings-people-page-index",
                                       "settings-account-page",
                                       "settings-sync-account-control",
                                       "#avatar-row"};

    auto steps =
        Steps(InstrumentTab(kActiveTab),
              NavigateWebContents(kActiveTab,
                                  GURL(chrome::kChromeUIAccountSettingsURL)),
              WaitForWebContentsPainted(kActiveTab),
              WaitForElementVisible(kActiveTab, kAvatarRowQuery));

    if (scroll_to_bottom) {
      const DeepQuery kOtherSyncItemsQuery = {
          "settings-ui", "settings-main#main", "settings-people-page-index",
          "settings-account-page#account", "#other-sync-items"};

      steps += Steps(ScrollIntoView(kActiveTab, kOtherSyncItemsQuery),
                     WaitForWebContentsPainted(kActiveTab));
    }

    const DeepQuery kSettingAccountPageQuery = {
        "settings-ui", "settings-main#main", "settings-people-page-index",
        "settings-account-page#account", "settings-subpage"};

    steps +=
        Steps(SetOnIncompatibleAction(
                  OnIncompatibleAction::kIgnoreAndContinue,
                  "Screenshots not supported in all testing environments."),
              ScreenshotWebUi(kActiveTab, kSettingAccountPageQuery,
                              screenshot_name, kScreenshotBaselineCL));

    return steps;
  }

  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

IN_PROC_BROWSER_TEST_F(AccountSettingsPagePixelBrowserTest,
                       OpenAccountSettingsPage) {
  SigninWithFullInfo();
  // All sync datatypes are enabled by default.

  RunTestSequence(NavigateToSettingsAndScreenshot("account_settings_page"));
}

IN_PROC_BROWSER_TEST_F(AccountSettingsPagePixelBrowserTest,
                       OpenAccountSettingsPageAdvancedSettings) {
  SigninWithFullInfo();

  // Scroll to the bottom to see advanced settings.
  RunTestSequence(
      NavigateToSettingsAndScreenshot("account_settings_page_advanced_settings",
                                      /*scroll_to_bottom=*/true));
}

IN_PROC_BROWSER_TEST_F(AccountSettingsPagePixelBrowserTest,
                       OpenAccountSettingsPageWithSyncOff) {
  SigninWithFullInfo();
  DisableAllSyncDatatypes();

  RunTestSequence(NavigateToSettingsAndScreenshot(
      "account_settings_page_sync_datatypes_off"));
}

IN_PROC_BROWSER_TEST_F(AccountSettingsPagePixelBrowserTest,
                       OpenAccountSettingsPageWithPassphraseRequired) {
  SigninWithFullInfo();
  SimulatePassphraseError();

  RunTestSequence(NavigateToSettingsAndScreenshot(
      "account_settings_page_passphrase_error"));
}

IN_PROC_BROWSER_TEST_F(AccountSettingsPagePixelBrowserTest,
                       OpenAccountSettingsPageWithPassphraseErrorCleared) {
  SigninWithFullInfo();
  SimulatePassphraseError();
  ClearPassphraseError();

  RunTestSequence(NavigateToSettingsAndScreenshot(
      "account_settings_page_passphrase_error_cleared",
      /*scroll_to_bottom=*/true));
}

IN_PROC_BROWSER_TEST_F(AccountSettingsPagePixelBrowserTest,
                       OpenAccountSettingsPageWithSyncDisabledByPolicy) {
  SigninWithFullInfo();
  GetTestSyncService()->SetAllowedByEnterprisePolicy(false);
  GetTestSyncService()->FireStateChanged();

  RunTestSequence(NavigateToSettingsAndScreenshot(
      "account_settings_page_sync_disabled_by_policy"));
}

IN_PROC_BROWSER_TEST_F(
    AccountSettingsPagePixelBrowserTest,
    OpenAccountSettingsPageWithSyncDatatypeDisabledByPolicy) {
  SigninWithFullInfo();

  // Disable settings sync datatype via policy.
  auto* settings = GetTestSyncService()->GetUserSettings();
  settings->SetSelectedType(syncer::UserSelectableType::kPreferences, false);
  settings->SetTypeIsManagedByPolicy(syncer::UserSelectableType::kPreferences,
                                     true);
  GetTestSyncService()->FireStateChanged();

  RunTestSequence(NavigateToSettingsAndScreenshot(
      "account_settings_page_sync_datatype_disabled_by_policy"));
}

}  // namespace
