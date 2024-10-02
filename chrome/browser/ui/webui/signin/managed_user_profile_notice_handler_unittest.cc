// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"

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
#if !BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#endif  //  !BUILDFLAG(IS_CHROMEOS)

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

class ManagedUserProfileNoticeHandlerTestBase
    : public BrowserWithTestWindowTest {
 public:
  ManagedUserProfileNoticeHandlerTestBase() = default;
  ManagedUserProfileNoticeHandlerTestBase(
      const ManagedUserProfileNoticeHandlerTestBase&) = delete;
  ManagedUserProfileNoticeHandler& operator=(
      const ManagedUserProfileNoticeHandlerTestBase&) = delete;
  ~ManagedUserProfileNoticeHandlerTestBase() override = default;

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

  void InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType screen_type,
      std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
          dialog_params) {
    message_handler_.reset();

    message_handler_ = std::make_unique<ManagedUserProfileNoticeHandler>(
        /*browser=*/nullptr, screen_type, std::move(dialog_params));
    message_handler_->set_web_ui_for_test(web_ui());
    message_handler_->RegisterMessages();
  }

  void DeleteHandler() { message_handler_.reset(); }

  void TearDown() override {
    message_handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  AccountInfo& account_info() { return account_info_; }
  ManagedUserProfileNoticeHandler* handler() { return message_handler_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  AccountInfo account_info_;

  std::unique_ptr<ManagedUserProfileNoticeHandler> message_handler_;
};

struct HandleProceedTestParam {
  ManagedUserProfileNoticeHandler::State state =
      ManagedUserProfileNoticeHandler::State::kDisclosure;
  bool profile_creation_required_by_policy = false;
  bool should_link_data = false;
  signin::SigninChoice expected_choice = signin::SIGNIN_CHOICE_CANCEL;
  signin::SigninChoiceOperationResult choice_operation_result =
      signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS;
};
const HandleProceedTestParam kHandleProceedParams[] = {
    {ManagedUserProfileNoticeHandler::State::kDisclosure, false, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_ERROR},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, false, true,
     signin::SIGNIN_CHOICE_CONTINUE,
     signin::SigninChoiceOperationResult::SIGNIN_ERROR},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, true, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_ERROR},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, true, true,
     signin::SIGNIN_CHOICE_CONTINUE,
     signin::SigninChoiceOperationResult::SIGNIN_ERROR},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, false, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, false, true,
     signin::SIGNIN_CHOICE_CONTINUE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, true, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, true, true,
     signin::SIGNIN_CHOICE_CONTINUE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, false, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, false, true,
     signin::SIGNIN_CHOICE_CONTINUE,
     signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, true, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT},
    {ManagedUserProfileNoticeHandler::State::kDisclosure, true, true,
     signin::SIGNIN_CHOICE_CONTINUE,
     signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT},
    {ManagedUserProfileNoticeHandler::State::kSuccess, false, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
    {ManagedUserProfileNoticeHandler::State::kError, false, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
    {ManagedUserProfileNoticeHandler::State::kTimeout, false, false,
     signin::SIGNIN_CHOICE_NEW_PROFILE,
     signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS},
};
// kTimeout, kProcessing, kError, kSuccess
class ManagedUserProfileNoticeHandleProceedTest
    : public ManagedUserProfileNoticeHandlerTestBase,
      public testing::WithParamInterface<HandleProceedTestParam> {};

// Tests how `HandleProceed` processes the arguments and the handler's state to
// notify the registered callback.
TEST_P(ManagedUserProfileNoticeHandleProceedTest, HandleProceed) {
  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info(),
          /*is_oidc_account=*/false,
          GetParam().profile_creation_required_by_policy,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));

  base::Value::List args;
  args.Append(GetParam().state);
  args.Append(GetParam().should_link_data);

  EXPECT_CALL(mock_process_user_choice_callback,
              Run(GetParam().expected_choice));
  EXPECT_CALL(mock_done_callback, Run());
  web_ui()->HandleReceivedMessage("proceed", args);
}

TEST_P(ManagedUserProfileNoticeHandleProceedTest,
       HandleProceedWithUserDataHandling) {
  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info(),
          /*is_oidc_account=*/false,
          GetParam().profile_creation_required_by_policy,
          /*show_link_data_option=*/true,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));

  base::Value::List args;
  args.Append(GetParam().state);
  args.Append(GetParam().should_link_data);

  EXPECT_CALL(mock_process_user_choice_callback,
              Run(GetParam().expected_choice));
  EXPECT_CALL(mock_done_callback, Run());
  web_ui()->HandleReceivedMessage("proceed", args);

  // When disclosure is shown, the next state shown is the data handling one.
  if (GetParam().state == ManagedUserProfileNoticeHandler::State::kDisclosure) {
    base::Value::List data_handling_args;
    data_handling_args.Append(
        ManagedUserProfileNoticeHandler::State::kUserDataHandling);
    data_handling_args.Append(GetParam().should_link_data);
    web_ui()->HandleReceivedMessage("proceed", data_handling_args);
  }
}

#if !BUILDFLAG(IS_CHROMEOS)
// Tests how `HandleProceed` processes the arguments and the handler's state to
// notify the registered callback.
TEST_P(ManagedUserProfileNoticeHandleProceedTest,
       HandleProceedWithDoneCallback) {
  base::test::ScopedFeatureList feature_list(
      profile_management::features::kOidcAuthProfileManagement);

  base::MockCallback<signin::SigninChoiceWithConfirmationCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info(),
          /*is_oidc_account=*/false,
          GetParam().profile_creation_required_by_policy,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));

  base::Value::List args;
  args.Append(ManagedUserProfileNoticeHandler::State::kDisclosure);
  args.Append(GetParam().should_link_data);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_process_user_choice_callback,
              Run(GetParam().expected_choice, ::testing::_))
      .WillOnce([&run_loop](
                    signin::SigninChoice choice,
                    signin::SigninChoiceOperationDoneCallback done_callback) {
        std::move(done_callback)
            .Run(signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
        run_loop.Quit();
      });
  EXPECT_CALL(mock_done_callback, Run());
  web_ui()->HandleReceivedMessage("proceed", args);
  run_loop.Run();
}

TEST_P(ManagedUserProfileNoticeHandleProceedTest,
       HandleProceedWithDoneCallbackAndUserDataHandling) {
  base::test::ScopedFeatureList feature_list(
      profile_management::features::kOidcAuthProfileManagement);

  base::MockCallback<signin::SigninChoiceWithConfirmationCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info(),
          /*is_oidc_account=*/false,
          GetParam().profile_creation_required_by_policy,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));

  base::Value::List args;
  args.Append(ManagedUserProfileNoticeHandler::State::kDisclosure);
  args.Append(GetParam().should_link_data);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_process_user_choice_callback,
              Run(GetParam().expected_choice, ::testing::_))
      .WillOnce([&run_loop](
                    signin::SigninChoice choice,
                    signin::SigninChoiceOperationDoneCallback done_callback) {
        std::move(done_callback)
            .Run(signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
        run_loop.Quit();
      });
  EXPECT_CALL(mock_done_callback, Run());
  web_ui()->HandleReceivedMessage("proceed", args);

  // When disclosure is shown, the next state shown is the data handling one.
  if (GetParam().state == ManagedUserProfileNoticeHandler::State::kDisclosure) {
    base::Value::List data_handling_args;
    data_handling_args.Append(
        ManagedUserProfileNoticeHandler::State::kUserDataHandling);
    data_handling_args.Append(GetParam().should_link_data);
    web_ui()->HandleReceivedMessage("proceed", data_handling_args);
  }

  run_loop.Run();
}

TEST_P(ManagedUserProfileNoticeHandleProceedTest,
       HandleProceedWithSuccessConfirmationCallback) {
  base::test::ScopedFeatureList feature_list(
      profile_management::features::kOidcAuthProfileManagement);
  base::MockCallback<signin::SigninChoiceWithConfirmationCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info(),
          /*is_oidc_account=*/false,
          GetParam().profile_creation_required_by_policy,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));

  base::Value::List args;
  args.Append(ManagedUserProfileNoticeHandler::State::kDisclosure);
  args.Append(GetParam().should_link_data);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_process_user_choice_callback,
              Run(GetParam().expected_choice, ::testing::_))
      .WillOnce(
          [&run_loop](signin::SigninChoice choice,
                      signin::SigninChoiceOperationDoneCallback done_callback) {
            std::move(done_callback).Run(GetParam().choice_operation_result);
            run_loop.Quit();
          });
  web_ui()->HandleReceivedMessage("proceed", args);
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_process_user_choice_callback);
  EXPECT_CALL(mock_done_callback, Run());
  web_ui()->HandleReceivedMessage("proceed", args);
}

#endif  //  !BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(All,
                         ManagedUserProfileNoticeHandleProceedTest,
                         testing::ValuesIn(kHandleProceedParams));

#if !BUILDFLAG(IS_CHROMEOS)
class ManagedUserProfileNoticeHandlerTest
    : public ManagedUserProfileNoticeHandlerTestBase {
 protected:
  ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }
};

TEST_F(ManagedUserProfileNoticeHandlerTest,
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
            &managed_profile, GetProfileEntry(&managed_profile),
            "intercepted.com", u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_STRICT_SEPARATION,
            u"example.com", u"alice@intercepted.com"));
  }
}

TEST_F(ManagedUserProfileNoticeHandlerTest,
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
    ManagedUserProfileNoticeHandlerTest,
    GetManagedAccountTitleWithEmailInterceptionEnforcedByInterceptedAccount) {
  auto& profile = profiles::testing::CreateProfileSync(
      profile_manager(), profile_manager()->GenerateNextProfileDirectoryPath());

  // No account manager
  {
    std::string title =
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
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
        ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
            &profile, GetProfileEntry(&profile), "intercepted.com",
            u"alice@intercepted.com");
    EXPECT_EQ(
        title,
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_SEPARATION,
            u"example.com", u"alice@intercepted.com", u"intercepted.com"));
  }
}
class ManagedUserProfileNoticeHandleCancelTest
    : public ManagedUserProfileNoticeHandlerTestBase {};

// Tests how `HandleCancel` processes the arguments and the handler's state to
// notify the registered callback.
TEST_F(ManagedUserProfileNoticeHandleCancelTest, HandleCancelNoUseAfterFree) {
  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  InitializeHandler(
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info(),
          /*is_oidc_account=*/false,
          /*profile_creation_required_by_policy=*/true,
          /*show_link_data_option=*/true,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));

  EXPECT_CALL(mock_process_user_choice_callback,
              Run(signin::SIGNIN_CHOICE_CANCEL))
      .WillOnce([&]() { DeleteHandler(); });
  EXPECT_CALL(mock_done_callback, Run());
  web_ui()->HandleReceivedMessage("cancel", base::Value::List());
  EXPECT_EQ(handler(), nullptr);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
