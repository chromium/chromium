// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::Invoke;

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
ProfileAttributesEntry* GetProfileEntry(Profile* profile) {
  return g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile->GetPath());
}
#endif

}  // namespace

class EnterpriseProfileWelcomeHandlerTestBase
    : public BrowserWithTestWindowTest {
 public:
  EnterpriseProfileWelcomeHandlerTestBase() = default;
  EnterpriseProfileWelcomeHandlerTestBase(
      const EnterpriseProfileWelcomeHandlerTestBase&) = delete;
  EnterpriseProfileWelcomeHandler& operator=(
      const EnterpriseProfileWelcomeHandlerTestBase&) = delete;
  ~EnterpriseProfileWelcomeHandlerTestBase() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    account_info_.email = user_manager::kStubUserEmail;
    account_info_.gaia = user_manager::kStubUserId;
    account_info_.account_id = CoreAccountId::FromGaiaId(account_info_.gaia);
  }

  void InitializeHandler(EnterpriseProfileWelcomeUI::ScreenType screen_type,
                         bool profile_creation_required_by_policy,
                         bool show_link_data_option,
                         signin::SigninChoiceCallback proceed_callback) {
    message_handler_.reset();

    message_handler_ = std::make_unique<EnterpriseProfileWelcomeHandler>(
        /*browser=*/nullptr, screen_type, profile_creation_required_by_policy,
        show_link_data_option, account_info_, std::move(proceed_callback));
    message_handler_->set_web_ui_for_test(web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    message_handler_.reset();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  EnterpriseProfileWelcomeHandler* handler() { return message_handler_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  AccountInfo account_info_;

  std::unique_ptr<EnterpriseProfileWelcomeHandler> message_handler_;
};

struct HandleProceedTestParam {
  bool profile_creation_required_by_policy = false;
  bool should_link_data = false;
  signin::SigninChoice expected_choice = signin::SIGNIN_CHOICE_CANCEL;
};
const HandleProceedTestParam kHandleProceedParams[] = {
    {false, false, signin::SIGNIN_CHOICE_NEW_PROFILE},
    {false, true, signin::SIGNIN_CHOICE_CONTINUE},
    {true, false, signin::SIGNIN_CHOICE_NEW_PROFILE},
    {true, true, signin::SIGNIN_CHOICE_CONTINUE},
};

class EnterpriseProfileWelcomeHandleProceedTest
    : public EnterpriseProfileWelcomeHandlerTestBase,
      public testing::WithParamInterface<HandleProceedTestParam> {};

// Tests how `HandleProceed` processes the arguments and the handler's state to
// notify the registered callback.
TEST_P(EnterpriseProfileWelcomeHandleProceedTest, HandleProceed) {
  base::MockCallback<signin::SigninChoiceCallback> mock_proceed_callback;
  InitializeHandler(
      EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled,
      GetParam().profile_creation_required_by_policy,
      /*show_link_data_option=*/true, mock_proceed_callback.Get());

  base::Value::List args;
  args.Append(GetParam().should_link_data);
  EXPECT_CALL(mock_proceed_callback, Run(GetParam().expected_choice));
  web_ui()->HandleReceivedMessage("proceed", args);
}

INSTANTIATE_TEST_SUITE_P(All,
                         EnterpriseProfileWelcomeHandleProceedTest,
                         testing::ValuesIn(kHandleProceedParams));

#if !BUILDFLAG(IS_CHROMEOS)
class EnterpriseProfileWelcomeHandleTest
    : public EnterpriseProfileWelcomeHandlerTestBase {
 protected:
  ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }
};

TEST_F(EnterpriseProfileWelcomeHandleTest,
       GetManagedAccountTitleWithEmailInterceptionEnforcedByExistingProfile) {
  auto& managed_profile = profiles::testing::CreateProfileSync(
      profile_manager(), profile_manager()->GenerateNextProfileDirectoryPath());
  GetProfileEntry(&managed_profile)->SetHostedDomain("example.com");

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(&managed_profile),
      policy::EnterpriseManagementAuthority::CLOUD);

  auto& unmanaged_profile = profiles::testing::CreateProfileSync(
      profile_manager(), profile_manager()->GenerateNextProfileDirectoryPath());

  managed_profile.GetPrefs()->SetList(
      prefs::kProfileSeparationDomainExceptionList, base::Value::List());
  unmanaged_profile.GetPrefs()->SetList(
      prefs::kProfileSeparationDomainExceptionList, base::Value::List());

  // No account manager, no device manager
  {
    const std::string unknown_device_manager = "";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        unknown_device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &unmanaged_profile, GetProfileEntry(&unmanaged_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED,
            u"alice@intercepted.com"));
  }
  // No account manager, existing device manager
  {
    const std::string device_manager = "devicemanager.com";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &unmanaged_profile, GetProfileEntry(&unmanaged_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED_BY,
            base::UTF8ToUTF16(device_manager), u"alice@intercepted.com"));
  }
  // Existing account manager, no device manager
  {
    const std::string unknown_device_manager = "";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        unknown_device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &managed_profile, GetProfileEntry(&managed_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_STRICT_SEPARATION,
            u"example.com", u"alice@intercepted.com"));
  }
  // Existing account manager and device manager
  {
    const std::string device_manager = "devicemanager.com";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &managed_profile, GetProfileEntry(&managed_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_STRICT_SEPARATION,
            u"example.com", u"alice@intercepted.com"));
  }
}

TEST_F(EnterpriseProfileWelcomeHandleTest,
       GetManagedAccountTitleWithEmailInterceptionEnforcedAtMachineLevel) {
  auto& managed_profile = profiles::testing::CreateProfileSync(
      profile_manager(), profile_manager()->GenerateNextProfileDirectoryPath());
  GetProfileEntry(&managed_profile)->SetHostedDomain("example.com");

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(&managed_profile),
      policy::EnterpriseManagementAuthority::CLOUD);

  auto& unmanaged_profile = profiles::testing::CreateProfileSync(
      profile_manager(), profile_manager()->GenerateNextProfileDirectoryPath());

  managed_profile.GetPrefs()->SetString(
      prefs::kManagedAccountsSigninRestriction, "primary_account");
  managed_profile.GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  unmanaged_profile.GetPrefs()->SetString(
      prefs::kManagedAccountsSigninRestriction, "primary_account");
  unmanaged_profile.GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);

  // No account manager, no device manager
  {
    const std::string unknown_device_manager = "";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        unknown_device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &unmanaged_profile, GetProfileEntry(&unmanaged_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED,
            u"alice@intercepted.com"));
  }
  // No account manager, existing device manager
  {
    const std::string device_manager = "devicemanager.com";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &unmanaged_profile, GetProfileEntry(&unmanaged_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED_BY,
            base::UTF8ToUTF16(device_manager), u"alice@intercepted.com"));
  }
  // Existing account manager, no device manager
  {
    const std::string unknown_device_manager = "";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        unknown_device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &managed_profile, GetProfileEntry(&managed_profile),
            "intercepted.com", u"alice@intercepted.com");
    l10n_util::GetStringFUTF8(
        IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED,
        u"alice@intercepted.com");
  }
  // Existing account manager and device manager
  {
    const std::string device_manager = "devicemanager.com";
    chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
        device_manager.c_str());
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &managed_profile, GetProfileEntry(&managed_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED_BY,
            base::UTF8ToUTF16(device_manager), u"alice@intercepted.com"));
  }
}

TEST_F(
    EnterpriseProfileWelcomeHandleTest,
    GetManagedAccountTitleWithEmailInterceptionEnforcedByInterceptedAccount) {
  auto& profile = profiles::testing::CreateProfileSync(
      profile_manager(), profile_manager()->GenerateNextProfileDirectoryPath());

  // No account manager
  {
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &profile, GetProfileEntry(&profile), "intercepted.com",
            u"alice@intercepted.com");
    EXPECT_EQ(title,
              l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_PROFILE_WELCOME_ACCOUNT_EMAIL_MANAGED_BY,
                  u"alice@intercepted.com", u"intercepted.com"));
  }
  // Known Account manager
  {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForProfile(&profile),
        policy::EnterpriseManagementAuthority::CLOUD);
    // Set account manager
    GetProfileEntry(&profile)->SetHostedDomain("example.com");
    std::string title =
        EnterpriseProfileWelcomeHandler::GetManagedAccountTitleWithEmail(
            &profile, GetProfileEntry(&profile), "intercepted.com",
            u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_SEPARATION,
            u"example.com", u"alice@intercepted.com", u"intercepted.com"));
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
