// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_state_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockHistorySyncOptinHelperDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  MOCK_METHOD(void, ShowHistorySyncOptinScreen, (), (override));
  MOCK_METHOD(void,
              ShowAccountManagementScreen,
              (signin::SigninChoiceCallback),
              (override));
  MOCK_METHOD(void, FinishFlowWithoutHistorySyncOptin, (), (override));
};

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}
}  // namespace

// TODO(crbug.com/434964019): When management screen support is implemented
// for the browser case, make this test parametrizable.
class HistorySyncOptinHelperBrowserTest : public SigninBrowserTestBase {
 public:
  HistorySyncOptinHelperBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

  AccountInfo MakeAccountInfoAvailable() {
    AccountInfo account_info =
        identity_test_env()->MakeAccountAvailable("test@example.com");
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    return account_info;
  }

  // Updates the fields relating to the account management.
  void UpdateAccountManagementInfo(AccountInfo& account_info, bool is_managed) {
    account_info.hosted_domain =
        is_managed ? "example.com" : signin::constants::kNoHostedDomainFound;
    CHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

 private:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));

    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    bool build_for_managed_account =
        std::string(test_info->name()).find("ForManagedAccount") !=
        std::string::npos;
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context,
        build_for_managed_account
            ? base::BindRepeating(
                  &policy::FakeUserPolicySigninService::BuildForEnterprise)
            : base::BindRepeating(&policy::FakeUserPolicySigninService::Build));
  }

  base::test::ScopedFeatureList scoped_features{
      switches::kEnableHistorySyncOptin};
};

IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinHelperBrowserTest,
    TriggersHistorySyncScreenWhenAccountInfoFetchedForConsumerAccount) {
  AccountInfo account_info = MakeAccountInfoAvailable();
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(1);

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInProfilePicker);
  history_sync_optin_helper.StartHistorySyncOptinFlow();

  // This triggers the flow that reaches the delegate's
  // `ShowHistorySyncOptinScreen`.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/false);

  // Subsequent updates should have no impact.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/false);
}

IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinHelperBrowserTest,
    TriggersManagedAccountScreenThenHistorySyncOptinScreenForManagedAccount) {
  AccountInfo account_info = MakeAccountInfoAvailable();
  MockHistorySyncOptinHelperDelegate delegate;

  base::RunLoop run_loop;
  // Mock accepting the user management screen.
  EXPECT_CALL(delegate, ShowAccountManagementScreen)
      .WillOnce([&](signin::SigninChoiceCallback callback) {
        std::move(callback).Run(signin::SIGNIN_CHOICE_NEW_PROFILE);
      });
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).WillOnce([&]() {
    run_loop.Quit();
  });

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInProfilePicker);
  history_sync_optin_helper.StartHistorySyncOptinFlow();

  // This triggers the flow that reaches the delegate's
  // `ShowAccountManagementScreen`.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
  run_loop.Run();

  // Subsequent updates should have no impact on the flow.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
}

IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinHelperBrowserTest,
    SkipsHistorySyncOptinScreenWhenUserRejectsManagementForManagedAccount) {
  AccountInfo account_info = MakeAccountInfoAvailable();
  MockHistorySyncOptinHelperDelegate delegate;

  base::RunLoop run_loop;
  // Mock cancelling the user management screen.
  EXPECT_CALL(delegate, ShowAccountManagementScreen)
      .WillOnce([&](signin::SigninChoiceCallback callback) {
        std::move(callback).Run(signin::SIGNIN_CHOICE_CANCEL);
      });
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);
  EXPECT_CALL(delegate, FinishFlowWithoutHistorySyncOptin).WillOnce([&]() {
    run_loop.Quit();
  });

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInProfilePicker);
  history_sync_optin_helper.StartHistorySyncOptinFlow();

  // This triggers the flow that reaches the delegate's
  // `ShowAccountManagementScreen`.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
  run_loop.Run();

  // Subsequent updates should have no impact on the flow.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);

}

IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinHelperBrowserTest,
    TriggersHistorySyncScreenWhenAccountInfoFetchingTimesOut) {
  AccountInfo account_info = MakeAccountInfoAvailable();
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInProfilePicker);
  history_sync_optin_helper.StartHistorySyncOptinFlow();
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(1);
  history_sync_optin_helper.GetAccountStateFetcherForTesting()
      ->EnforceTimeoutReachedForTesting();

  // After the timeout is reached, account info updates should have no impact.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
}

IN_PROC_BROWSER_TEST_F(HistorySyncOptinHelperBrowserTest,
                       WaitsForSyncServiceBeforeTriggeringHistorySyncScreen) {
  // Set the sync service in pending state.
  GetTestSyncService()->SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  AccountInfo account_info = MakeAccountInfoAvailable();
  UpdateAccountManagementInfo(account_info, /*is_managed=*/false);
  MockHistorySyncOptinHelperDelegate delegate;

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInProfilePicker);

  // The helper is waiting for the sync service to start before attempting
  // to show the history sync optin screen.
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);
  history_sync_optin_helper.StartHistorySyncOptinFlow();
  testing::Mock::VerifyAndClearExpectations(&delegate);
  EXPECT_TRUE(
      history_sync_optin_helper.GetSyncServiceStartupStateObserverForTesting());

  // When sync becomes active, thepl flow resumes to showing the history sync
  // optin screen.
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(1);
  GetTestSyncService()->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  GetTestSyncService()->FireStateChanged();
}

IN_PROC_BROWSER_TEST_F(HistorySyncOptinHelperBrowserTest,
                       SkipsHistorySyncOptinScreenWhenSyncIsDisabled) {
  // Disable the sync service.
  GetTestSyncService()->SetAllowedByEnterprisePolicy(false);

  AccountInfo account_info = MakeAccountInfoAvailable();
  UpdateAccountManagementInfo(account_info, /*is_managed=*/false);
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);
  EXPECT_CALL(delegate, FinishFlowWithoutHistorySyncOptin).Times(1);

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInProfilePicker);
  history_sync_optin_helper.StartHistorySyncOptinFlow();
}
