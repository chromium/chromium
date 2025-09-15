// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockHistorySyncOptinHelperDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  MOCK_METHOD(void,
              ShowHistorySyncOptinScreen,
              (Profile*, base::OnceClosure history_optin_completed_closure),
              (override));
  MOCK_METHOD(void,
              ShowAccountManagementScreen,
              (signin::SigninChoiceCallback),
              (override));
  MOCK_METHOD(void, FinishFlowWithoutHistorySyncOptin, (), (override));
};

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
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
    profile_ = builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    service_ = std::make_unique<HistorySyncOptinService>(profile_.get());
  }

  AccountInfo MakePrimaryAccountAvailable() {
    AccountInfo account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("test@gmail.com",
                                          signin::ConsentLevel::kSignin);
    account_info.hosted_domain = signin::constants::kNoHostedDomainFound;
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
    return account_info;
  }

  ~HistorySyncOptinServiceTest() override = default;

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
  AccountInfo account_info = MakePrimaryAccountAvailable();
  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .Times(1);
  bool flow_started =
      service_->StartHistorySyncOptinFlow(account_info, std::move(delegate));
  EXPECT_TRUE(flow_started);
}

TEST_F(HistorySyncOptinServiceTest, AbortFlowIfOneInProgress) {
  AccountInfo account_info = MakePrimaryAccountAvailable();
  auto delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* delegate_ptr = delegate.get();

  base::OnceClosure captured_closure;
  EXPECT_CALL(*delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .WillOnce(testing::Invoke(
          [&](Profile* profile,
              base::OnceClosure history_optin_completed_closure) {
            captured_closure = std::move(history_optin_completed_closure);
          }));

  // Start the first flow.
  bool flow_started =
      service_->StartHistorySyncOptinFlow(account_info, std::move(delegate));
  EXPECT_TRUE(flow_started);

  // A second flow cannot be started.
  flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::make_unique<MockHistorySyncOptinHelperDelegate>());
  EXPECT_FALSE(flow_started);

  // Complete the first flow.
  std::move(captured_closure).Run();

  // After the previous flow finished a new one can be started.
  auto second_delegate = std::make_unique<MockHistorySyncOptinHelperDelegate>();
  auto* second_delegate_ptr = second_delegate.get();
  EXPECT_CALL(*second_delegate_ptr,
              ShowHistorySyncOptinScreen(profile_.get(), testing::_))
      .Times(1);
  flow_started = service_->StartHistorySyncOptinFlow(
      account_info, std::move(second_delegate));
  EXPECT_TRUE(flow_started);
}
