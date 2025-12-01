// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_promo_util.h"
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
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "history_sync_optin_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockHistorySyncOptinHelperDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  MOCK_METHOD(void,
              ShowHistorySyncOptinScreen,
              (Profile*,
               HistorySyncOptinHelper::FlowCompletedCallback
                   history_optin_completed_callback),
              (override));
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

class MockProfileManagementDisclaimerService
    : public ProfileManagementDisclaimerService {
 public:
  explicit MockProfileManagementDisclaimerService(Profile* profile)
      : ProfileManagementDisclaimerService(profile) {}
  ~MockProfileManagementDisclaimerService() override = default;

  MOCK_METHOD(void,
              EnsureManagedProfileForAccount,
              (const CoreAccountId& account_id,
               signin_metrics::AccessPoint access_point,
               base::OnceCallback<void(Profile*, bool)> callback),
              (override));
};

std::unique_ptr<KeyedService> BuildMockProfileManagementDisclaimerService(
    content::BrowserContext* context) {
  return std::make_unique<MockProfileManagementDisclaimerService>(
      Profile::FromBrowserContext(context));
}

class HistorySyncOptinHelperTestObserver
    : public HistorySyncOptinHelper::Observer {
 public:
  explicit HistorySyncOptinHelperTestObserver(
      base::test::TestFuture<void>& future)
      : future_(future) {}

  // HistorySyncOptinHelper::Observer implementation:
  void OnHistorySyncOptinHelperFlowFinished() override { future_->SetValue(); }

  ~HistorySyncOptinHelperTestObserver() override = default;

 private:
  base::raw_ref<base::test::TestFuture<void>> future_;
};

class HistorySyncOptinHelperBrowserTest : public SigninBrowserTestBase {
 public:
  HistorySyncOptinHelperBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

  AccountInfo MakeAccountInfoAvailableAndSignIn() {
    AccountInfo account_info =
        identity_test_env()->MakeAccountAvailable("test@example.com");
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    identity_test_env()->UpdateAccountInfoForAccount(account_info);

    identity_test_env()->SetPrimaryAccount(account_info.email,
                                           signin::ConsentLevel::kSignin);
    return account_info;
  }

  // Updates the fields relating to the account management.
  void UpdateAccountManagementInfo(AccountInfo& account_info, bool is_managed) {
    account_info =
        AccountInfo::Builder(account_info)
            .SetHostedDomain(is_managed ? "example.com" : std::string())
            .Build();
    CHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

  MockProfileManagementDisclaimerService*
  GetProfileManagementDisclaimerService() {
    return static_cast<MockProfileManagementDisclaimerService*>(
        ProfileManagementDisclaimerServiceFactory::GetForProfile(GetProfile()));
  }

 protected:
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;

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

    ProfileManagementDisclaimerServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&BuildMockProfileManagementDisclaimerService));
  }

  base::test::ScopedFeatureList scoped_features_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

class HistorySyncOptinHelperLaunchContextParamBrowserTest
    : public HistorySyncOptinHelperBrowserTest,
      public testing::WithParamInterface<
          HistorySyncOptinHelper::LaunchContext> {};

IN_PROC_BROWSER_TEST_P(
    HistorySyncOptinHelperLaunchContextParamBrowserTest,
    TriggersHistorySyncScreenWhenAccountInfoFetchedForConsumerAccount) {
  GetTestSyncService()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen(GetProfile(), testing::_))
      .Times(1);

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, GetParam(), signin_metrics::AccessPoint::kSettings);
  history_sync_optin_helper->StartHistorySyncOptinFlow();
  // This triggers the flow that reaches the delegate's
  // `ShowHistorySyncOptinScreen`.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/false);

  // Subsequent updates should have no impact.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/false);

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kSettings,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Skipped",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Aborted",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.AlreadyOptedIn",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Completed",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Declined",
                                     /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_P(
    HistorySyncOptinHelperLaunchContextParamBrowserTest,
    TriggersManagedAccountScreenThenHistorySyncOptinScreenForManagedAccount) {
  GetTestSyncService()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();
  MockHistorySyncOptinHelperDelegate delegate;

  base::test::TestFuture<Profile*> future;

  auto* service = GetProfileManagementDisclaimerService();
  // Mock accepting the user management screen.
  switch (GetParam()) {
    case HistorySyncOptinHelper::LaunchContext::kInBrowser:
      EXPECT_CALL(*service, EnsureManagedProfileForAccount)
          .WillOnce(
              [&](const CoreAccountId&, signin_metrics::AccessPoint,
                  base::OnceCallback<void(Profile*, bool)> callback) {
                // Mark management as accepted.
                enterprise_util::SetUserAcceptedAccountManagement(GetProfile(),
                                                                  true);
                // The callback is executed asynchronously, to better reflect
                // the production implementation.
                base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), GetProfile(), true));
              });
      break;
    case HistorySyncOptinHelper::LaunchContext::kInProfilePicker:
      EXPECT_CALL(delegate, ShowAccountManagementScreen)
          .WillOnce([&](signin::SigninChoiceCallback callback) {
            std::move(callback).Run(signin::SIGNIN_CHOICE_NEW_PROFILE);
          });
      break;
  }

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen)
      .WillOnce([&](Profile* profile,
                    HistorySyncOptinHelper::FlowCompletedCallback
                        history_optin_completed_callback) {
        future.SetValue(profile);
      });

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, GetParam(), signin_metrics::AccessPoint::kSettings);
  history_sync_optin_helper->StartHistorySyncOptinFlow();

  // This triggers the flow that reaches the delegate's
  // `ShowAccountManagementScreen`.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
  EXPECT_EQ(future.Get(), GetProfile());

  // Subsequent updates should have no impact on the flow.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
}

IN_PROC_BROWSER_TEST_P(
    HistorySyncOptinHelperLaunchContextParamBrowserTest,
    SkipsHistorySyncOptinScreenWhenUserRejectsManagementForManagedAccount) {
  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();
  MockHistorySyncOptinHelperDelegate delegate;

  base::test::TestFuture<void> future;

  // Mock cancelling the user management screen.
  auto* service = GetProfileManagementDisclaimerService();
  switch (GetParam()) {
    case HistorySyncOptinHelper::LaunchContext::kInBrowser:
      EXPECT_CALL(*service, EnsureManagedProfileForAccount)
          .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                        base::OnceCallback<void(Profile*, bool)> callback) {
            // The callback is executed asynchronously, to better reflect
            // the production implementation.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), nullptr, false));
          });
      break;
    case HistorySyncOptinHelper::LaunchContext::kInProfilePicker:
      EXPECT_CALL(delegate, ShowAccountManagementScreen)
          .WillOnce([&](signin::SigninChoiceCallback callback) {
            std::move(callback).Run(signin::SIGNIN_CHOICE_CANCEL);
          });
      break;
  }
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, GetParam(), signin_metrics::AccessPoint::kSettings);
  HistorySyncOptinHelperTestObserver history_sync_optin_helper_observer(future);
  history_sync_optin_helper->AddObserver(&history_sync_optin_helper_observer);

  history_sync_optin_helper->StartHistorySyncOptinFlow();

  // This triggers the flow that reaches the delegate's
  // `ShowAccountManagementScreen`.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
  EXPECT_TRUE(future.Wait());

  // Subsequent updates should have no impact on the flow.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);

  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Skipped",
      /*sample=*/signin_metrics::AccessPoint::kSettings,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Started",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Aborted",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.AlreadyOptedIn",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Completed",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Declined",
                                     /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_P(
    HistorySyncOptinHelperLaunchContextParamBrowserTest,
    TriggersHistorySyncScreenWhenAccountInfoFetchingTimesOut) {
  GetTestSyncService()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();

  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, GetParam(), signin_metrics::AccessPoint::kSettings);
  history_sync_optin_helper->StartHistorySyncOptinFlow();
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen(GetProfile(), testing::_))
      .Times(1);
  history_sync_optin_helper->GetAccountStateFetcherForTesting()
      ->EnforceTimeoutReachedForTesting();

  // After the timeout is reached, account info updates should have no impact.
  UpdateAccountManagementInfo(account_info, /*is_managed=*/true);
}

IN_PROC_BROWSER_TEST_P(HistorySyncOptinHelperLaunchContextParamBrowserTest,
                       WaitsForSyncServiceBeforeTriggeringHistorySyncScreen) {
  GetTestSyncService()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Set the sync service in pending state.
  GetTestSyncService()->SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();
  UpdateAccountManagementInfo(account_info, false);
  MockHistorySyncOptinHelperDelegate delegate;

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, GetParam(), signin_metrics::AccessPoint::kSettings);

  // The helper is waiting for the sync service to start before attempting
  // to show the history sync optin screen.
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);
  history_sync_optin_helper->StartHistorySyncOptinFlow();
  testing::Mock::VerifyAndClearExpectations(&delegate);
  EXPECT_TRUE(history_sync_optin_helper
                  ->GetSyncServiceStartupStateObserverForTesting());

  // When sync becomes active, thepl flow resumes to showing the history sync
  // optin screen.
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen(GetProfile(), testing::_))
      .Times(1);
  GetTestSyncService()->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  GetTestSyncService()->FireStateChanged();

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kSettings,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Skipped",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Aborted",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.AlreadyOptedIn",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Completed",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Declined",
                                     /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_P(HistorySyncOptinHelperLaunchContextParamBrowserTest,
                       SkipsHistorySyncOptinScreenWhenSyncIsDisabled) {
  // Disable the sync service.
  GetTestSyncService()->SetAllowedByEnterprisePolicy(false);

  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();
  UpdateAccountManagementInfo(account_info, false);
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);
  EXPECT_CALL(delegate, FinishFlowWithoutHistorySyncOptin).Times(1);

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, GetParam(), signin_metrics::AccessPoint::kSettings);
  history_sync_optin_helper->StartHistorySyncOptinFlow();

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);

  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Skipped",
      /*sample=*/signin_metrics::AccessPoint::kSettings,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Started",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Aborted",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.AlreadyOptedIn",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Completed",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Declined",
                                     /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HistorySyncOptinHelperLaunchContextParamBrowserTest,
    testing::Values(HistorySyncOptinHelper::LaunchContext::kInProfilePicker,
                    HistorySyncOptinHelper::LaunchContext::kInBrowser),
    [](const testing::TestParamInfo<HistorySyncOptinHelper::LaunchContext>&
           info) {
      return info.param ==
                     HistorySyncOptinHelper::LaunchContext::kInProfilePicker
                 ? "InPicker"
                 : "InBrowser";
    });

IN_PROC_BROWSER_TEST_F(HistorySyncOptinHelperBrowserTest,
                       CompletedFromAvatarPillAccessPoint) {
  AccountInfo account_info = MakeAccountInfoAvailableAndSignIn();
  UpdateAccountManagementInfo(account_info, false);
  GetTestSyncService()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Simulate the promo being shown twice.
  signin::SyncPromoIdentityPillManager pill_manager(
      identity_manager(), browser()->profile()->GetPrefs());
  pill_manager.RecordPromoShown(
      signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo);
  pill_manager.RecordPromoShown(
      signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo);

  MockHistorySyncOptinHelperDelegate delegate;
  // Accept History Sync.
  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen)
      .WillOnce([](Profile*, HistorySyncOptinHelper::FlowCompletedCallback
                                 history_optin_completed_callback) {
        std::move(history_optin_completed_callback.value())
            .Run(HistorySyncOptinHelper::ScreenChoiceResult::kAccepted);
      });

  auto history_sync_optin_helper = HistorySyncOptinHelper::Create(
      identity_test_env()->identity_manager(), GetProfile(), account_info,
      &delegate, HistorySyncOptinHelper::LaunchContext::kInBrowser,
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup);
  history_sync_optin_helper->StartHistorySyncOptinFlow();

  histogram_tester_.ExpectBucketCount(
      "Signin.AvatarPillPromo.AcceptedAtShownCount.HistorySync", /*sample=*/2,
      /*expected_count=*/1);

  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Aborted",
      /*sample=*/
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Completed",
      /*sample=*/
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
}
