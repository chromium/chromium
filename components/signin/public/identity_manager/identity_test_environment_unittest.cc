// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_environment.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class IdentityTestEnvironmentTest : public testing::Test {
 public:
  IdentityTestEnvironmentTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  ~IdentityTestEnvironmentTest() override { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironmentTest);
};

TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentCancelsPendingRequestsOnDestruction) {
  std::unique_ptr<IdentityTestEnvironment> identity_test_environment =
      std::make_unique<IdentityTestEnvironment>();

  identity_test_environment->MakePrimaryAccountAvailable("primary@example.com");
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::set<std::string> scopes{"scope"};

  std::unique_ptr<AccessTokenFetcher> fetcher =
      identity_test_environment->identity_manager()
          ->CreateAccessTokenFetcherForAccount(
              identity_test_environment->identity_manager()
                  ->GetPrimaryAccountId(),
              "dummy_consumer", scopes, std::move(callback),
              AccessTokenFetcher::Mode::kImmediate);

  // Deleting the IdentityTestEnvironment should cancel any pending
  // task in order to avoid use-after-free crashes. The destructor of
  // the test will spin the runloop which would run
  // IdentityTestEnvironment pending tasks if not canceled.
  identity_test_environment.reset();
  fetcher.reset();
}

}  // namespace signin
