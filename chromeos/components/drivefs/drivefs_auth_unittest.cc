// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_auth.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

constexpr char kTestEmail[] = "test@example.com";
constexpr base::TimeDelta kTokenLifetime = base::TimeDelta::FromHours(1);

class AuthDelegateImpl : public DriveFsAuth::Delegate {
 public:
  AuthDelegateImpl(signin::IdentityManager* identity_manager,
                   const AccountId& account_id)
      : identity_manager_(identity_manager), account_id_(account_id) {}

  ~AuthDelegateImpl() override = default;

 private:
  // AuthDelegate::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  signin::IdentityManager* GetIdentityManager() override {
    return identity_manager_;
  }
  const AccountId& GetAccountId() override { return account_id_; }
  std::string GetObfuscatedAccountId() override {
    return "salt-" + account_id_.GetAccountIdKey();
  }

  bool IsMetricsCollectionEnabled() override { return false; }

  signin::IdentityManager* const identity_manager_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(AuthDelegateImpl);
};

class DriveFsAuthTest : public ::testing::Test {
 public:
  DriveFsAuthTest() = default;

 protected:
  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    identity_test_env_.MakeUnconsentedPrimaryAccountAvailable(kTestEmail);
    auto timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    delegate_ = std::make_unique<AuthDelegateImpl>(
        identity_test_env_.identity_manager(),
        AccountId::FromUserEmailGaiaId(kTestEmail, "ID"));
    auth_ = std::make_unique<DriveFsAuth>(&clock_,
                                          base::FilePath("/path/to/profile"),
                                          std::move(timer), delegate_.get());
  }

  void TearDown() override {
    EXPECT_FALSE(timer_->IsRunning());
    auth_.reset();
  }

  // Helper function for better line wrapping.
  void RespondWithAccessToken(const std::string& token) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        token, clock_.Now() + kTokenLifetime);
  }

  // Helper function for better line wrapping.
  void RespondWithAuthError(GoogleServiceAuthError::State error_state) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        GoogleServiceAuthError(error_state));
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::SimpleTestClock clock_;

  std::unique_ptr<AuthDelegateImpl> delegate_;
  std::unique_ptr<DriveFsAuth> auth_;
  base::MockOneShotTimer* timer_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsAuthTest);
};

TEST_F(DriveFsAuthTest, GetAccessToken_Success) {
  base::RunLoop run_loop;
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                            const std::string& token) {
        EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
        EXPECT_EQ("auth token", token);
        run_loop.Quit();
      }));
  RespondWithAccessToken("auth token");
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Permanent) {
  base::RunLoop run_loop;
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                            const std::string& token) {
        EXPECT_EQ(mojom::AccessTokenStatus::kAuthError, status);
        EXPECT_TRUE(token.empty());
        run_loop.Quit();
      }));
  RespondWithAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Transient) {
  base::RunLoop run_loop;
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                            const std::string& token) {
        EXPECT_EQ(mojom::AccessTokenStatus::kTransientError, status);
        EXPECT_TRUE(token.empty());
        run_loop.Quit();
      }));
  RespondWithAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Timeout) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting(
                 [&](mojom::AccessTokenStatus status, const std::string&) {
                   EXPECT_EQ(mojom::AccessTokenStatus::kAuthError, status);
                   std::move(quit_closure).Run();
                 }));
  // Timer fires before access token becomes available.
  timer_->Fire();
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_TimeoutRace) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting(
                 [&](mojom::AccessTokenStatus status, const std::string&) {
                   EXPECT_EQ(mojom::AccessTokenStatus::kAuthError, status);
                   std::move(quit_closure).Run();
                 }));
  // Timer fires before access token becomes available.
  timer_->Fire();
  // Timer callback should stop access token retrieval.
  RespondWithAccessToken("auth token");
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_ParallelRequests) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                            const std::string& token) {
        EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
        EXPECT_EQ("auth token", token);
        std::move(quit_closure).Run();
      }));
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                            const std::string& token) {
        EXPECT_EQ(mojom::AccessTokenStatus::kTransientError, status);
        EXPECT_TRUE(token.empty());
      }));
  RespondWithAccessToken("auth token");
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_SequentialRequests) {
  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    auth_->GetAccessToken(
        false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                              const std::string& token) {
          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
          EXPECT_EQ("auth token", token);
          run_loop.Quit();
        }));
    RespondWithAccessToken("auth token");
    run_loop.Run();
  }
  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    auth_->GetAccessToken(
        false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                              const std::string& token) {
          EXPECT_EQ(mojom::AccessTokenStatus::kAuthError, status);
          EXPECT_TRUE(token.empty());
          run_loop.Quit();
        }));
    RespondWithAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    run_loop.Run();
  }
}

TEST_F(DriveFsAuthTest, Caching) {
  auth_->GetAccessToken(true, base::BindOnce([](mojom::AccessTokenStatus status,
                                                const std::string& token) {
                          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                          EXPECT_EQ("auth token", token);
                        }));
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());
  RespondWithAccessToken("auth token");

  // Second attempt should reuse already available token.
  auth_->GetAccessToken(true, base::BindOnce([](mojom::AccessTokenStatus status,
                                                const std::string& token) {
                          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                          EXPECT_EQ("auth token", token);
                        }));
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
}

TEST_F(DriveFsAuthTest, CachedAndNotCached) {
  auth_->GetAccessToken(true, base::BindOnce([](mojom::AccessTokenStatus status,
                                                const std::string& token) {
                          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                          EXPECT_EQ("auth token", token);
                        }));
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());
  RespondWithAccessToken("auth token");

  // Second attempt should reuse already available token.
  auth_->GetAccessToken(true, base::BindOnce([](mojom::AccessTokenStatus status,
                                                const std::string& token) {
                          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                          EXPECT_EQ("auth token", token);
                        }));
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());

  // Now ask for token explicitly bypassing the cache.
  auth_->GetAccessToken(
      false, base::BindOnce(
                 [](mojom::AccessTokenStatus status, const std::string& token) {
                   EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                   EXPECT_EQ("auth token 2", token);
                 }));
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());
  RespondWithAccessToken("auth token 2");
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
}

TEST_F(DriveFsAuthTest, CacheExpired) {
  auth_->GetAccessToken(true, base::BindOnce([](mojom::AccessTokenStatus status,
                                                const std::string& token) {
                          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                          EXPECT_EQ("auth token", token);
                        }));
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());
  RespondWithAccessToken("auth token");

  clock_.Advance(base::TimeDelta::FromHours(2));

  // The token expired so a new one is requested.
  auth_->GetAccessToken(true, base::BindOnce([](mojom::AccessTokenStatus status,
                                                const std::string& token) {
                          EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
                          EXPECT_EQ("auth token 2", token);
                        }));
  RespondWithAccessToken("auth token 2");
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
}

}  // namespace
}  // namespace drivefs
