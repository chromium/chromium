// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_environment.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class IdentityTestEnvironmentTest : public testing::Test {
 public:
  IdentityTestEnvironmentTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  IdentityTestEnvironmentTest(const IdentityTestEnvironmentTest&) = delete;
  IdentityTestEnvironmentTest& operator=(const IdentityTestEnvironmentTest&) =
      delete;

  ~IdentityTestEnvironmentTest() override { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentCancelsPendingRequestsOnDestruction) {
  std::unique_ptr<IdentityTestEnvironment> identity_test_environment =
      std::make_unique<IdentityTestEnvironment>();

  identity_test_environment->MakePrimaryAccountAvailable(
      "primary@example.com", signin::ConsentLevel::kSignin);
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::set<std::string> scopes{GaiaConstants::kGoogleUserInfoEmail};

  std::unique_ptr<AccessTokenFetcher> fetcher =
      identity_test_environment->identity_manager()
          ->CreateAccessTokenFetcherForAccount(
              identity_test_environment->identity_manager()
                  ->GetPrimaryAccountId(ConsentLevel::kSignin),
              "dummy_consumer", scopes, std::move(callback),
              AccessTokenFetcher::Mode::kImmediate);

  // Deleting the IdentityTestEnvironment should cancel any pending
  // task in order to avoid use-after-free crashes. The destructor of
  // the test will spin the runloop which would run
  // IdentityTestEnvironment pending tasks if not canceled.
  identity_test_environment.reset();
  fetcher.reset();
}

// TODO(https://crbug.com/1462552): Delete this test once `ConsentLevel::kSync`
// is deleted.
TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentSetPrimaryAccountWithSyncConsent) {
  std::unique_ptr<IdentityTestEnvironment> identity_test_environment =
      std::make_unique<IdentityTestEnvironment>();
  IdentityManager* identity_manager =
      identity_test_environment->identity_manager();
  std::string primary_account_email = "primary@example.com";

  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
  AccountInfo account_info =
      identity_test_environment->MakeAccountAvailable(primary_account_email);
  identity_test_environment->SetPrimaryAccount(primary_account_email,
                                               ConsentLevel::kSync);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
}

TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentSetPrimaryAccountWithoutSyncConsent) {
  std::unique_ptr<IdentityTestEnvironment> identity_test_environment =
      std::make_unique<IdentityTestEnvironment>();
  IdentityManager* identity_manager =
      identity_test_environment->identity_manager();
  std::string primary_account_email = "primary@example.com";

  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  AccountInfo account_info =
      identity_test_environment->MakeAccountAvailable(primary_account_email);
  identity_test_environment->SetPrimaryAccount(primary_account_email,
                                               ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));

  // TODO(https://crbug.com/1462552): Remove once `ConsentLevel::kSync` is
  // deleted.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
}

// TODO(https://crbug.com/1462552): Delete this test once `ConsentLevel::kSync`
// is deleted.
TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentMakePrimaryAccountAvailableWithSyncConsent) {
  std::unique_ptr<IdentityTestEnvironment> identity_test_environment =
      std::make_unique<IdentityTestEnvironment>();
  IdentityManager* identity_manager =
      identity_test_environment->identity_manager();
  std::string primary_account_email = "primary@example.com";

  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
  identity_test_environment->MakePrimaryAccountAvailable(primary_account_email,
                                                         ConsentLevel::kSync);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
}

TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentMakePrimaryAccountAvailableWithoutSyncConsent) {
  std::unique_ptr<IdentityTestEnvironment> identity_test_environment =
      std::make_unique<IdentityTestEnvironment>();
  IdentityManager* identity_manager =
      identity_test_environment->identity_manager();
  std::string primary_account_email = "primary@example.com";

  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  identity_test_environment->MakePrimaryAccountAvailable(primary_account_email,
                                                         ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));

  // TODO(https://crbug.com/1462552): Remove once `ConsentLevel::kSync` is
  // deleted.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
}

}  // namespace signin
