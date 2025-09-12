// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockHistorySyncOptinHelperDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  MOCK_METHOD(void, ShowHistorySyncOptinScreen, (Profile*), (override));
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
    feature_list_.InitAndEnableFeature(switches::kEnableHistorySyncOptin);
    TestingProfile::Builder builder;
    builder.AddTestingFactories({IdentityTestEnvironmentProfileAdaptor::
                                     GetIdentityTestEnvironmentFactories()});
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildTestSyncService));
    profile_ = builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    service_ = std::make_unique<HistorySyncOptinService>(profile_.get());
    delegate_ = std::make_unique<MockHistorySyncOptinHelperDelegate>();
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
  EXPECT_CALL(*delegate_, ShowHistorySyncOptinScreen(profile_.get())).Times(1);
  service_->StartHistorySyncOptinFlow(account_info, std::move(delegate_));
}
