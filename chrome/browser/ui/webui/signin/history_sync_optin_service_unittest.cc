// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kMainEmail[] = "main_email@gmail.com";
const char kManagedEmail[] = "managed_account@example.com";
const char kManagedEmail2[] = "managed_account2@example.com";
}  // namespace

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

class MockHistorySyncOptinService : public HistorySyncOptinService {
 public:
  explicit MockHistorySyncOptinService(Profile* profile)
      : HistorySyncOptinService(profile) {}
  ~MockHistorySyncOptinService() override = default;

  MOCK_METHOD(void, ShowErrorDialogWithMessage, (int), (override));
};

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

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> BuildHistorySyncOptinService(
    content::BrowserContext* context) {
  return std::make_unique<MockHistorySyncOptinService>(
      Profile::FromBrowserContext(context));
}

class ResetObserver : public HistorySyncOptinService::Observer {
 public:
  explicit ResetObserver(HistorySyncOptinService* history_sync_optin_service) {
    CHECK(history_sync_optin_service);
    observation_.Observe(history_sync_optin_service);
  }

  void WaitForReset() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  ~ResetObserver() override = default;

  void StopObserving() { observation_.Reset(); }

 private:
  void OnHistorySyncOptinServiceReset() override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;
  base::ScopedObservation<HistorySyncOptinService,
                          HistorySyncOptinService::Observer>
      observation_{this};
};

class CrashingObserver : public HistorySyncOptinHelper::Observer {
 public:
  explicit CrashingObserver(HistorySyncOptinHelper* helper) : helper_(helper) {
    observation_.Observe(helper);
  }

  void OnHistorySyncOptinHelperFlowFinished() override {
    // If the HistorySyncOptinServer had already deleted the helper on the first
    // observer's `OnHistorySyncOptinHelperFlowFinished` invocation then this
    // would crash.
    helper_->GetAccountStateFetcherForTesting();
  }

 private:
  raw_ptr<HistorySyncOptinHelper> helper_;
  base::ScopedObservation<HistorySyncOptinHelper,
                          HistorySyncOptinHelper::Observer>
      observation_{this};
};

class HistorySyncOptinServiceTestBase : public testing::Test {
 public:
  HistorySyncOptinServiceTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    feature_list_.InitAndEnableFeature(
        syncer::kReplaceSyncPromosWithSignInPromos);
    CHECK(profile_manager_.SetUp());
    profile_ = CreateNewProfile(profile_manager_, "TestingProfile");
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    service_ = static_cast<MockHistorySyncOptinService*>(
        HistorySyncOptinServiceFactory::GetForProfile(profile_.get()));
  }

  AccountInfo MakePrimaryAccountAvailable(
      std::string email,
      IdentityTestEnvironmentProfileAdaptor* identity_test_env_adaptor,
      bool with_managed_account_info_available,
      std::optional<signin_metrics::AccessPoint> access_point = std::nullopt) {
    auto builder = identity_test_env_adaptor->identity_test_env()
                       ->CreateAccountAvailabilityOptionsBuilder();
    if (access_point.has_value()) {
      builder.WithAccessPoint(access_point.value());
    }

    AccountInfo account_info =
        identity_test_env_adaptor->identity_test_env()->MakeAccountAvailable(
            builder.AsPrimary(signin::ConsentLevel::kSignin).Build(email));

    if (with_managed_account_info_available) {
      UpdateAccountManagementInfo(account_info, identity_test_env_adaptor);
    }
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    identity_test_env_adaptor->identity_test_env()->UpdateAccountInfoForAccount(
        account_info);
    return account_info;
  }

  void UpdateAccountManagementInfo(
      AccountInfo& account_info,
      IdentityTestEnvironmentProfileAdaptor* identity_test_env_adaptor) {
    std::string hosted_domain;
    if (account_info.email == kManagedEmail ||
        account_info.email == kManagedEmail2) {
      hosted_domain = "example.com";
    } else {
      CHECK_EQ(account_info.email, kMainEmail);
    }

    account_info = AccountInfo::Builder(account_info)
                       .SetHostedDomain(hosted_domain)
                       .Build();

    identity_test_env_adaptor->identity_test_env()->UpdateAccountInfoForAccount(
        account_info);
  }

  ~HistorySyncOptinServiceTestBase() override = default;

  void DisableHistorySync(Profile* profile) {
    auto* sync_service = SyncServiceFactory::GetForProfile(profile);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, /*is_type_on=*/false);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, /*is_type_on=*/false);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups, /*is_type_on=*/false);
  }

  TestingProfile* CreateNewProfile(TestingProfileManager& profile_manager,
                                   std::string_view profile_name) {
    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    testing_factories.emplace_back(SyncServiceFactory::GetInstance(),
                                   base::BindRepeating(&BuildTestSyncService));
    testing_factories.emplace_back(
        HistorySyncOptinServiceFactory::GetInstance(),
        base::BindRepeating(&BuildHistorySyncOptinService));
    testing_factories.emplace_back(
        ProfileManagementDisclaimerServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockProfileManagementDisclaimerService));
    TestingProfile* profile = profile_manager.CreateTestingProfile(
        std::string(profile_name), std::move(testing_factories), nullptr);
    return profile;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<MockHistorySyncOptinHelperDelegate> delegate_;
  raw_ptr<HistorySyncOptinService> service_;
};

class HistorySyncOptinServiceTest : public HistorySyncOptinServiceTestBase,
                                    public testing::WithParamInterface<bool> {
 public:
  bool IsManagedAccountInfoAvailableInAdvance() { return GetParam(); }
};

TEST_P(HistorySyncOptinServiceTest, StartFlow) {
  AccountInfo account_info =
      MakePrimaryAccountAvailable(kMainEmail, identity_test_env_adaptor_.get(),
                                  IsManagedAccountInfoAvailableInAdvance());
  SyncServiceFactory::GetForProfile(profile_.get())
      ->GetUserSettings()
      ->SetSelectedTypes(
          /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .Times(1);
  bool flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);
  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(account_info, identity_test_env_adaptor_.get());
  }
}

TEST_P(HistorySyncOptinServiceTest, AbortFlowIfOneInProgress) {
  AccountInfo account_info =
      MakePrimaryAccountAvailable(kMainEmail, identity_test_env_adaptor_.get(),
                                  IsManagedAccountInfoAvailableInAdvance());
  SyncServiceFactory::GetForProfile(profile_.get())
      ->GetUserSettings()
      ->SetSelectedTypes(
          /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();

  HistorySyncOptinHelper::FlowCompletedCallback captured_callback;
  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .WillOnce([&](Profile* profile,
                    HistorySyncOptinHelper::FlowCompletedCallback
                        history_optin_completed_callback) {
        captured_callback = std::move(history_optin_completed_callback);
      });

  // Start the first flow.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);
  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(account_info, identity_test_env_adaptor_.get());
  }

  // A second flow cannot be started.
  flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::make_unique<MockHistorySyncOptinHelperDelegate>(),
      signin_metrics::AccessPoint::kSettings);
  EXPECT_FALSE(flow_started);

  ResetObserver service_observer(service_.get());
  // Complete the first flow.
  std::move(captured_callback.value())
      .Run(HistorySyncOptinHelper::ScreenChoiceResult::kAccepted);
  // Wait for the synchronous reset of the service_'s state.
  service_observer.WaitForReset();

  // After the previous flow finished a new one can be started.
  auto second_delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* second_delegate_ptr = second_delegate.get();
  EXPECT_CALL(*second_delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .Times(1);
  flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::move(second_delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);
}

// Tests that when a new managed profile is created as a result of accepting
// management tearing down the service tied to the originating profile due not
// affect the history sync flow of the new profile, which proceeds normally.
TEST_P(HistorySyncOptinServiceTest,
       ShowsManagementScreenThenHistorySyncOnNewProfile) {
  base::test::TestFuture<void> future;
  base::HistogramTester histogram_tester;

  // Sign-in with the managed user account to the existing `profile_`.
  AccountInfo original_managed_account_info = MakePrimaryAccountAvailable(
      kManagedEmail, identity_test_env_adaptor_.get(),
      IsManagedAccountInfoAvailableInAdvance());

  // Create a new managed profile that will be used during profile management's
  // new profile selection (during the `EnsureManagedProfileForAccount`
  // execution).
  auto* new_managed_profile =
      CreateNewProfile(profile_manager_, "NewManagedProfile");

  // Do not sync history, tabs and tab groups.
  DisableHistorySync(new_managed_profile);
  DisableHistorySync(profile_);

  IdentityTestEnvironmentProfileAdaptor new_profile_adaptor(
      new_managed_profile);

  auto* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(
              profile_.get()));

  ResetObserver service_observer(service_.get());

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        // Sign the user in to the new managed profile.
        MakePrimaryAccountAvailable(original_managed_account_info.email,
                                    &new_profile_adaptor,
                                    IsManagedAccountInfoAvailableInAdvance());
        signin::AccountsMutator* account_mutator =
            identity_test_env_adaptor_->identity_test_env()
                ->identity_manager()
                ->GetAccountsMutator();
        account_mutator->MoveAccount(new_profile_adaptor.identity_test_env()
                                         ->identity_manager()
                                         ->GetAccountsMutator(),
                                     original_managed_account_info.account_id);

        CHECK(!identity_test_env_adaptor_->identity_test_env()
                   ->identity_manager()
                   ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
        CHECK(new_profile_adaptor.identity_test_env()
                  ->identity_manager()
                  ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
        // Mark management as accepted.
        enterprise_util::SetUserAcceptedAccountManagement(new_managed_profile,
                                                          true);

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), new_managed_profile, true));
        // The service is shutdown after the callback above has run.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindLambdaForTesting([&]() { service_->Shutdown(); }));

        // Trigger an update of the account info. It should have no effect on
        // the flow (i.e. no double triggering of any method).
        AccountInfo updated_info =
            new_profile_adaptor.identity_test_env()
                ->identity_manager()
                ->FindExtendedAccountInfo(original_managed_account_info);
        updated_info.full_name = "updated name";
        new_profile_adaptor.identity_test_env()->UpdateAccountInfoForAccount(
            updated_info);
      });

  auto original_delegate =
      std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* original_delegate_ptr = original_delegate.get();
  EXPECT_CALL(*original_delegate_ptr,
              ShowHistorySyncOptinScreen(testing::_, testing::_))
      .Times(0);

  auto new_profile_delegate =
      std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* new_profile_delegate_ptr = new_profile_delegate.get();
  HistorySyncOptinServiceFactory::GetForProfile(new_managed_profile)
      ->SetDelegateForTesting(std::move(new_profile_delegate));

  // The service attached to the new managed profile should resume the flow and
  // invoke the history sync screen.
  EXPECT_CALL(*new_profile_delegate_ptr,
              ShowHistorySyncOptinScreen(new_managed_profile, testing::_))
      .WillOnce(testing::InvokeWithoutArgs([&future] { future.SetValue(); }));

  // Start the history sync opt-in flow with the managed account.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      original_managed_account_info, std::move(original_delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);

  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(original_managed_account_info,
                                identity_test_env_adaptor_.get());
  }
  EXPECT_TRUE(future.Wait());
  // Wait for the original service to be reset.
  service_observer.WaitForReset();
  service_observer.StopObserving();

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_bucket_count=*/1);
}

// Tests that the history sync optin flow is resumed on the existing profile
// after the user accepts management.
TEST_P(HistorySyncOptinServiceTest,
       ShowsManagementScreenThenHistorySyncOnCurrentProfile) {
  base::test::TestFuture<void> future;
  base::HistogramTester histogram_tester;

  // Sign-in with the managed user account to the existing `profile_`.
  AccountInfo managed_account_info = MakePrimaryAccountAvailable(
      kManagedEmail, identity_test_env_adaptor_.get(),
      IsManagedAccountInfoAvailableInAdvance());

  // Do not sync history, tabs and tab groups.
  DisableHistorySync(profile_);

  auto* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(profile_));

  ResetObserver service_observer(service_.get());

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        // Mark management as accepted and proceed the management flow on the
        // current profile.
        enterprise_util::SetUserAcceptedAccountManagement(profile_, true);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), profile_, true));
      });

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();

  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .WillOnce([&](Profile* profile,
                    HistorySyncOptinHelper::FlowCompletedCallback callback) {
        std::move(callback.value())
            .Run(HistorySyncOptinHelper::ScreenChoiceResult::kDeclined);
        future.SetValue();
      });

  // Start the history sync opt-in flow with the managed account.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      managed_account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);

  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(managed_account_info,
                                identity_test_env_adaptor_.get());
  }
  EXPECT_TRUE(future.Wait());
  // Wait for the original service to be reset.
  service_observer.WaitForReset();
  service_observer.StopObserving();

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_bucket_count=*/1);
}

// Regression test for crbug.com/452313094, to ensure flows for managed accounts
// invoke only once the HistorySyncOptinHelper::ShowHistorySyncOptinScreen.
TEST_P(HistorySyncOptinServiceTest,
       MakesSingleHistorySyncOptinScreenInvocation) {
  base::test::TestFuture<void> future;
  base::HistogramTester histogram_tester;

  // Sign-in with the managed user account to the existing `profile_`.
  AccountInfo original_managed_account_info = MakePrimaryAccountAvailable(
      kManagedEmail, identity_test_env_adaptor_.get(),
      IsManagedAccountInfoAvailableInAdvance());

  // Do not sync history, tabs and tab groups.
  DisableHistorySync(profile_);

  auto* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(profile_));

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        // Mark management as accepted.
        enterprise_util::SetUserAcceptedAccountManagement(profile_, true);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), profile_, true));
      });

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr, ShowHistorySyncOptinScreen(testing::_, testing::_))
      .WillOnce(testing::InvokeWithoutArgs([&future] { future.SetValue(); }));

  // Start the history sync opt-in flow with the managed account.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      original_managed_account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);
  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(original_managed_account_info,
                                identity_test_env_adaptor_.get());
  }
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_bucket_count=*/1);
}

// Regression test ensuring that the service doesn't destruct prematurely it's
// objects (including the helper), while they are still in use.
TEST_P(HistorySyncOptinServiceTest,
       MultipleObserversDoNotCrashOnFlowCompletion) {
  AccountInfo account_info =
      MakePrimaryAccountAvailable(kMainEmail, identity_test_env_adaptor_.get(),
                                  IsManagedAccountInfoAvailableInAdvance());
  DisableHistorySync(profile_);

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();

  HistorySyncOptinHelper::FlowCompletedCallback captured_callback;
  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .WillOnce([&](Profile* profile,
                    HistorySyncOptinHelper::FlowCompletedCallback
                        history_optin_completed_callback) {
        captured_callback = std::move(history_optin_completed_callback);
      });

  service_->StartHistorySyncOptinFlow(
      account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(account_info, identity_test_env_adaptor_.get());
  }
  HistorySyncOptinHelper* helper =
      service_->GetHistorySyncOptinHelperForTesting();
  ASSERT_TRUE(helper);

  CrashingObserver observer(helper);

  std::move(captured_callback)
      .value()
      .Run(HistorySyncOptinHelper::ScreenChoiceResult::kAccepted);
  // Completing the flow results in destructing the helper, but this should
  // happen only when it is no longer in use.
}

TEST_P(HistorySyncOptinServiceTest,
       AbortsFlowOnManagedUserProfileCreationConflict) {
  base::HistogramTester histogram_tester;
  TestingProfile::TestingFactories testing_factories =
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories();
  testing_factories.emplace_back(SyncServiceFactory::GetInstance(),
                                 base::BindRepeating(&BuildTestSyncService));
  testing_factories.emplace_back(
      HistorySyncOptinServiceFactory::GetInstance(),
      base::BindRepeating(&BuildHistorySyncOptinService));
  TestingProfile* new_managed_profile = profile_manager_.CreateTestingProfile(
      "ManagedProfile", std::move(testing_factories), nullptr);

  IdentityTestEnvironmentProfileAdaptor new_profile_adaptor(
      new_managed_profile);

  auto* disclaimer_service =
      ProfileManagementDisclaimerServiceFactory::GetForProfile(
          new_managed_profile);

  // Sign-in with the managed user account to the new profile and setup the
  // disclaimer service (real instance) to see this account as the candidate for
  // management. Do not make the management info known to the disclaimer service
  // yet, so that the `original_managed_account_info` remains the candidate for
  // creating a managed profile.
  bool with_managed_account_info_available = false;
  AccountInfo original_managed_account_info = MakePrimaryAccountAvailable(
      kManagedEmail, &new_profile_adaptor, with_managed_account_info_available);
  disclaimer_service->EnsureManagedProfileForAccount(
      original_managed_account_info.account_id,
      signin_metrics::AccessPoint::kSettings, base::DoNothing());
  ASSERT_EQ(disclaimer_service->GetAccountBeingConsideredForManagementIfAny(),
            original_managed_account_info.account_id);

  // Make the second managed account available.
  AccountInfo other_account_info =
      new_profile_adaptor.identity_test_env()->MakeAccountAvailable(
          kManagedEmail2);
  new_profile_adaptor.identity_test_env()->UpdateAccountInfoForAccount(
      other_account_info);
  if (IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(other_account_info, &new_profile_adaptor);
  }

  // The disclaimer service still sees the first account as the candidate for
  // management.
  ASSERT_EQ(disclaimer_service->GetAccountBeingConsideredForManagementIfAny(),
            original_managed_account_info.account_id);

  auto* history_sync_optin_service =
      HistorySyncOptinServiceFactory::GetForProfile(new_managed_profile);
  ResetObserver service_observer(history_sync_optin_service);

  // Start the history sync opt-in flow with the second managed account.
  // During the management flow a collision should be detected and the flow
  // should be aborted.
  bool flow_started = history_sync_optin_service->StartHistorySyncOptinFlow(
      other_account_info,
      std::make_unique<MockHistorySyncOptinHelperDelegate>(),
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
  EXPECT_TRUE(flow_started);

  if (!IsManagedAccountInfoAvailableInAdvance()) {
    UpdateAccountManagementInfo(other_account_info, &new_profile_adaptor);
  }
  // The history sync optin service is reset when the flow is aborted.
  service_observer.WaitForReset();
  service_observer.StopObserving();
  EXPECT_FALSE(
      history_sync_optin_service->GetHistorySyncOptinHelperForTesting());

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Aborted",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.ManagedUserProfileCreationConflict", true, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HistorySyncOptinServiceTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "WithManagementInfoKnownInAdvance"
                                      : "WithManagementInfoFetchedLater";
                         });

struct ManagedDataTypeTestParam {
  syncer::UserSelectableType data_type;
  signin_metrics::AccessPoint access_point;
};

class ManagedDataTypeErrorScreenServiceTest
    : public HistorySyncOptinServiceTestBase,
      public testing::WithParamInterface<
          std::tuple<ManagedDataTypeTestParam, bool>> {
 public:
  bool IsManagedAccountInfoAvailableInAdvance() {
    return std::get<1>(GetParam());
  }
};

TEST_P(ManagedDataTypeErrorScreenServiceTest,
       OnSiginInShowErrorIfDatatypeSyncIsManagedOnCurrentProfile) {
  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{});
  sync_service->GetUserSettings()->SetTypeIsManagedByPolicy(
      std::get<0>(GetParam()).data_type, /*managed=*/true);

  base::test::TestFuture<void> future;
  MockHistorySyncOptinService* history_sync_optin_service =
      static_cast<MockHistorySyncOptinService*>(service_);
  EXPECT_CALL(*history_sync_optin_service,
              ShowErrorDialogWithMessage(testing::_))
      .WillOnce(testing::InvokeWithoutArgs([&future] { future.SetValue(); }));

  MockProfileManagementDisclaimerService* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(
              profile_.get()));

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        std::move(callback).Run(profile_.get(), true);
      });

  // Sigin the user to the browser. This triggers the flow to enable tab syncing
  // and shows an error screen (due to syncing disabled by policies).
  MakePrimaryAccountAvailable(kManagedEmail, identity_test_env_adaptor_.get(),
                              IsManagedAccountInfoAvailableInAdvance(),
                              std::get<0>(GetParam()).access_point);
  EXPECT_TRUE(future.Wait());
}

TEST_P(ManagedDataTypeErrorScreenServiceTest,
       OnSiginInShowErrorIfDatatypeSyncIsManagedOnNewProfile) {
  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{});

  MockProfileManagementDisclaimerService* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(
              profile_.get()));

  // Create a new managed profile that will be used during profile management's
  // new profile selection (during the `EnsureManagedProfileForAccount`
  // execution).
  TestingProfile* new_managed_profile =
      CreateNewProfile(profile_manager_, "NewManagedProfile");
  CHECK(new_managed_profile);
  syncer::TestSyncService* new_sync_service =
      static_cast<syncer::TestSyncService*>(
          SyncServiceFactory::GetForProfile(new_managed_profile));
  new_sync_service->GetUserSettings()->SetTypeIsManagedByPolicy(
      std::get<0>(GetParam()).data_type, /*managed=*/true);

  // Do not sync history, tabs and tab groups.
  DisableHistorySync(new_managed_profile);

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId& account_id,
                    signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        IdentityTestEnvironmentProfileAdaptor new_profile_adaptor(
            new_managed_profile);
        // Sign in the account to the new managed profile.
        MakePrimaryAccountAvailable(kManagedEmail, &new_profile_adaptor,
                                    IsManagedAccountInfoAvailableInAdvance());
        // Mark management as accepted.
        enterprise_util::SetUserAcceptedAccountManagement(new_managed_profile,
                                                          true);
        std::move(callback).Run(new_managed_profile, true);
      });

  base::test::TestFuture<void> future;
  MockHistorySyncOptinService* new_history_sync_optin_service =
      static_cast<MockHistorySyncOptinService*>(
          HistorySyncOptinServiceFactory::GetForProfile(new_managed_profile));
  EXPECT_CALL(*new_history_sync_optin_service,
              ShowErrorDialogWithMessage(testing::_))
      .WillOnce(testing::InvokeWithoutArgs([&future] { future.SetValue(); }));

  // Sigin the user to the browser. This triggers the flow to enable tab syncing
  // and shows an error screen (due to syncing disabled by policies).
  MakePrimaryAccountAvailable(kManagedEmail, identity_test_env_adaptor_.get(),
                              IsManagedAccountInfoAvailableInAdvance(),
                              std::get<0>(GetParam()).access_point);
  EXPECT_TRUE(future.Wait());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManagedDataTypeErrorScreenServiceTest,
    testing::Combine(
        testing::Values(
            ManagedDataTypeTestParam(syncer::UserSelectableType::kTabs,
                                     signin_metrics::AccessPoint::kRecentTabs),
            ManagedDataTypeTestParam(
                syncer::UserSelectableType::kSavedTabGroups,
                signin_metrics::AccessPoint::kCollaborationJoinTabGroup),
            ManagedDataTypeTestParam(
                syncer::UserSelectableType::kSavedTabGroups,
                signin_metrics::AccessPoint::kCollaborationShareTabGroup)),
        testing::Bool()));
