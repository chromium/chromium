// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
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
  return std::make_unique<HistorySyncOptinService>(
      Profile::FromBrowserContext(context));
}

class HistorySyncOptinServiceTest : public testing::Test {
 public:
  HistorySyncOptinServiceTest() {
    feature_list_.InitAndEnableFeature(
        syncer::kReplaceSyncPromosWithSignInPromos);
    TestingProfile::Builder builder;
    builder.AddTestingFactories({IdentityTestEnvironmentProfileAdaptor::
                                     GetIdentityTestEnvironmentFactories()});
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildTestSyncService));
    builder.AddTestingFactory(
        ProfileManagementDisclaimerServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockProfileManagementDisclaimerService));

    profile_ = builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    service_ = std::make_unique<HistorySyncOptinService>(profile_.get());
  }

  AccountInfo MakePrimaryAccountAvailable(
      std::string email,
      IdentityTestEnvironmentProfileAdaptor* identity_test_env_adaptor) {
    AccountInfo account_info =
        identity_test_env_adaptor->identity_test_env()
            ->MakePrimaryAccountAvailable(email, signin::ConsentLevel::kSignin);

    if (email == kMainEmail) {
      account_info.hosted_domain = signin::constants::kNoHostedDomainFound;
    } else if (email == kManagedEmail || email == kManagedEmail2) {
      account_info.hosted_domain = "example.com";
    } else {
      NOTREACHED();
    }
    identity_test_env_adaptor->identity_test_env()->UpdateAccountInfoForAccount(
        account_info);
    return account_info;
  }

  ~HistorySyncOptinServiceTest() override = default;

  void DisableHistorySync(Profile* profile) {
    auto* sync_service = SyncServiceFactory::GetForProfile(profile);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, /*is_type_on=*/false);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, /*is_type_on=*/false);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups, /*is_type_on=*/false);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<MockHistorySyncOptinHelperDelegate> delegate_;
  std::unique_ptr<HistorySyncOptinService> service_;
};

TEST_F(HistorySyncOptinServiceTest, StartFlow) {
  AccountInfo account_info =
      MakePrimaryAccountAvailable(kMainEmail, identity_test_env_adaptor_.get());
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
      signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_TRUE(flow_started);
}

TEST_F(HistorySyncOptinServiceTest, AbortFlowIfOneInProgress) {
  AccountInfo account_info =
      MakePrimaryAccountAvailable(kMainEmail, identity_test_env_adaptor_.get());
  SyncServiceFactory::GetForProfile(profile_.get())
      ->GetUserSettings()
      ->SetSelectedTypes(
          /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();

  HistorySyncOptinHelper::FlowCompletedCallback captured_callback;
  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .WillOnce(testing::Invoke(
          [&](Profile* profile, HistorySyncOptinHelper::FlowCompletedCallback
                                    history_optin_completed_callback) {
            captured_callback = std::move(history_optin_completed_callback);
          }));

  // Start the first flow.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_TRUE(flow_started);

  // A second flow cannot be started.
  flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::make_unique<MockHistorySyncOptinHelperDelegate>(),
      signin_metrics::AccessPoint::kSettings);
  EXPECT_FALSE(flow_started);

  // Complete the first flow.
  std::move(captured_callback.value())
      .Run(HistorySyncOptinHelper::ScreenChoiceResult::kAccepted);

  // After the previous flow finished a new one can be started.
  auto second_delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* second_delegate_ptr = second_delegate.get();
  EXPECT_CALL(*second_delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .Times(1);
  flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::move(second_delegate),
      signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_TRUE(flow_started);
}

// Tests that when a new managed profile is created as a result of accepting
// management tearing down the service tied to the originating profile due not
// affect the history sync flow of the new profile, which proceeds normally.
TEST_F(HistorySyncOptinServiceTest,
       FlowInProgressDuringOriginalProfileTeardown) {
  base::HistogramTester histogram_tester;

  // Sign-in with the managed user account to the existing `profile_`.
  AccountInfo original_managed_account_info = MakePrimaryAccountAvailable(
      kManagedEmail, identity_test_env_adaptor_.get());

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create a new managed profile that will be used during profile management's
  // new profile selection (during the `EnsureManagedProfileForAccount`
  // execution).
  TestingProfile::TestingFactories testing_factories =
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories();
  testing_factories.emplace_back(SyncServiceFactory::GetInstance(),
                                 base::BindRepeating(&BuildTestSyncService));
  testing_factories.emplace_back(
      HistorySyncOptinServiceFactory::GetInstance(),
      base::BindRepeating(&BuildHistorySyncOptinService));
  TestingProfile* new_managed_profile = profile_manager.CreateTestingProfile(
      "NewManagedProfile", std::move(testing_factories), nullptr);

  // Do not sync history, tabs and tab groups.
  DisableHistorySync(new_managed_profile);
  DisableHistorySync(profile_.get());

  IdentityTestEnvironmentProfileAdaptor new_profile_adaptor(
      new_managed_profile);

  base::OnceCallback<void(Profile*, bool)> captured_callback;
  auto* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(
              profile_.get()));

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        MakePrimaryAccountAvailable(original_managed_account_info.email,
                                    &new_profile_adaptor);
        captured_callback = std::move(callback);
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
      .Times(1);

  // Start the history sync opt-in flow with the managed account.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      original_managed_account_info, std::move(original_delegate),
      signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_TRUE(flow_started);

  ASSERT_TRUE(new_profile_adaptor.identity_test_env()
                  ->identity_manager()
                  ->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  ASSERT_TRUE(captured_callback);
  std::move(captured_callback).Run(new_managed_profile, true);
  // Destroy the service tied to the original profile. It should not affect the
  // flow.
  service_->Shutdown();
  service_.reset();

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_bucket_count=*/1);
}

// Regression test for crbug.com/452313094, to ensure flows for managed accounts
// invoke only once the HistorySyncOptinHelper::ShowHistorySyncOptinScreen.
TEST_F(HistorySyncOptinServiceTest,
       MakesSingleHistorySyncOptinScreenInvocation) {
  base::HistogramTester histogram_tester;

  // Sign-in with the managed user account to the existing `profile_`.
  AccountInfo original_managed_account_info = MakePrimaryAccountAvailable(
      kManagedEmail, identity_test_env_adaptor_.get());

  // Do not sync history, tabs and tab groups.
  DisableHistorySync(profile_.get());

  auto* disclaimer_service =
      static_cast<MockProfileManagementDisclaimerService*>(
          ProfileManagementDisclaimerServiceFactory::GetForProfile(
              profile_.get()));

  EXPECT_CALL(*disclaimer_service, EnsureManagedProfileForAccount)
      .WillOnce([&](const CoreAccountId&, signin_metrics::AccessPoint,
                    base::OnceCallback<void(Profile*, bool)> callback) {
        std::move(callback).Run(profile_.get(), true);
      });

  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr, ShowHistorySyncOptinScreen(testing::_, testing::_))
      .Times(1);

  // Start the history sync opt-in flow with the managed account.
  bool flow_started = service_->StartHistorySyncOptinFlow(
      original_managed_account_info, std::move(delegate),
      signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_TRUE(flow_started);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_bucket_count=*/1);
}
