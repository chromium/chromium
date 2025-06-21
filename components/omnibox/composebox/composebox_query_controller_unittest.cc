// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kTestUser[] = "test_user@gmail.com";

class ComposeboxQueryControllerTest : public testing::Test {
 public:
  ComposeboxQueryControllerTest() = default;
  ~ComposeboxQueryControllerTest() override = default;

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);

    controller_ = std::make_unique<TestComposeboxQueryController>(
        identity_manager(), shared_url_loader_factory_,
        version_info::Channel::UNKNOWN);
  }

  TestComposeboxQueryController& controller() { return *controller_; }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  const signin::AccessTokenInfo& access_token_info() const {
    return access_token_info_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<TestComposeboxQueryController> controller_;
  signin::AccessTokenInfo access_token_info_{"access_token", base::Time::Max(),
                                             "id_token"};
};

TEST_F(ComposeboxQueryControllerTest, NotifySessionStarted) {
  controller().NotifySessionStarted();
  EXPECT_EQ(SessionState::kSessionStarted, controller().session_state());
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequest) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  run_loop.Run();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequestWithOAuth) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
  run_loop.Run();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequestFailure) {
  // Wait until the state changes to kClusterInfoInvalid.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoInvalid) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().set_next_cluster_info_request_should_return_error(true);
  controller().NotifySessionStarted();
  run_loop.Run();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoInvalid,
            controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest, NotifySessionAbandoned) {
  controller().NotifySessionAbandoned();
  EXPECT_EQ(SessionState::kSessionAbandoned, controller().session_state());
}
