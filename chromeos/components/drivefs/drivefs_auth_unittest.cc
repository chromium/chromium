// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_auth.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/identity/public/mojom/identity_accessor.mojom-test-utils.h"
#include "services/identity/public/mojom/identity_service.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

class AuthDelegateImpl : public DriveFsAuth::Delegate {
 public:
  AuthDelegateImpl(identity::mojom::IdentityService* identity_service,
                   const AccountId& account_id)
      : identity_service_(identity_service), account_id_(account_id) {}

  ~AuthDelegateImpl() override = default;

 private:
  // AuthDelegate::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  void BindIdentityAccessor(
      mojo::PendingReceiver<identity::mojom::IdentityAccessor> receiver)
      override {
    identity_service_->BindIdentityAccessor(std::move(receiver));
  }
  const AccountId& GetAccountId() override { return account_id_; }
  std::string GetObfuscatedAccountId() override {
    return "salt-" + account_id_.GetAccountIdKey();
  }

  bool IsMetricsCollectionEnabled() override { return false; }

  identity::mojom::IdentityService* const identity_service_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(AuthDelegateImpl);
};

class MockIdentityAccessor {
 public:
  MOCK_METHOD3(
      GetAccessToken,
      std::pair<base::Optional<std::string>, GoogleServiceAuthError::State>(
          const std::string& account_id,
          const ::identity::ScopeSet& scopes,
          const std::string& consumer_id));

  mojo::ReceiverSet<identity::mojom::IdentityAccessor>* receivers_ = nullptr;
};

class FakeIdentityService
    : public identity::mojom::IdentityAccessorInterceptorForTesting,
      public identity::mojom::IdentityService {
 public:
  explicit FakeIdentityService(MockIdentityAccessor* mock,
                               const base::Clock* clock)
      : mock_(mock), clock_(clock) {
    mock_->receivers_ = &receivers_;
  }

  ~FakeIdentityService() override { mock_->receivers_ = nullptr; }

  void set_auth_enabled(bool enabled) { auth_enabled_ = enabled; }

 private:
  // identity::mojom::IdentityService:
  void BindIdentityAccessor(
      mojo::PendingReceiver<identity::mojom::IdentityAccessor> receiver)
      override {
    receivers_.Add(this, std::move(receiver));
  }

  // identity::mojom::IdentityAccessorInterceptorForTesting overrides:
  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override {
    if (!auth_enabled_) {
      return;
    }
    auto account_id = AccountId::FromUserEmailGaiaId("test@example.com", "ID");
    std::move(callback).Run(CoreAccountId(account_id.GetUserEmail()),
                            account_id.GetGaiaId(), account_id.GetUserEmail(),
                            {});
  }

  void GetAccessToken(const CoreAccountId& account_id,
                      const ::identity::ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override {
    auto result = mock_->GetAccessToken(account_id, scopes, consumer_id);
    std::move(callback).Run(std::move(result.first),
                            clock_->Now() + base::TimeDelta::FromSeconds(1),
                            GoogleServiceAuthError(result.second));
  }

  IdentityAccessor* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MockIdentityAccessor* const mock_;
  const base::Clock* const clock_;
  mojo::ReceiverSet<identity::mojom::IdentityAccessor> receivers_;
  bool auth_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeIdentityService);
};

class DriveFsAuthTest : public ::testing::Test {
 public:
  DriveFsAuthTest() = default;

 protected:
  void SetUp() override {
    account_id_ = AccountId::FromUserEmailGaiaId("test@example.com", "ID");
    clock_.SetNow(base::Time::Now());
    identity_service_ = std::make_unique<FakeIdentityService>(
        &mock_identity_accessor_, &clock_);
    auto timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    delegate_ = std::make_unique<AuthDelegateImpl>(identity_service_.get(),
                                                   account_id_);
    auth_ = std::make_unique<DriveFsAuth>(&clock_,
                                          base::FilePath("/path/to/profile"),
                                          std::move(timer), delegate_.get());
  }

  void TearDown() override {
    EXPECT_FALSE(timer_->IsRunning());
    auth_.reset();
  }

  void ExpectAccessToken(bool use_cached,
                         mojom::AccessTokenStatus expected_status,
                         const std::string& expected_token) {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auth_->GetAccessToken(use_cached, base::BindLambdaForTesting(
                                          [&](mojom::AccessTokenStatus status,
                                              const std::string& token) {
                                            EXPECT_EQ(expected_status, status);
                                            EXPECT_EQ(expected_token, token);
                                            std::move(quit_closure).Run();
                                          }));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  MockIdentityAccessor mock_identity_accessor_;
  base::SimpleTestClock clock_;
  std::unique_ptr<FakeIdentityService> identity_service_;

  AccountId account_id_;

  std::unique_ptr<AuthDelegateImpl> delegate_;
  std::unique_ptr<DriveFsAuth> auth_;
  base::MockOneShotTimer* timer_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsAuthTest);
};

TEST_F(DriveFsAuthTest, GetAccessToken_Success) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));
  ExpectAccessToken(false, mojom::AccessTokenStatus::kSuccess, "auth token");
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Permanent) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(std::make_pair(
          base::nullopt, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)));
  ExpectAccessToken(false, mojom::AccessTokenStatus::kAuthError, "");
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Transient) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(std::make_pair(
          base::nullopt, GoogleServiceAuthError::SERVICE_UNAVAILABLE)));
  ExpectAccessToken(false, mojom::AccessTokenStatus::kTransientError, "");
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Timeout) {
  identity_service_->set_auth_enabled(false);
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting(
                 [&](mojom::AccessTokenStatus status, const std::string&) {
                   EXPECT_EQ(mojom::AccessTokenStatus::kAuthError, status);
                   std::move(quit_closure).Run();
                 }));
  timer_->Fire();
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_ParallelRequests) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));
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
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_SequentialRequests) {
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(mock_identity_accessor_,
                GetAccessToken("test@example.com", _, "drivefs"))
        .WillOnce(testing::Return(
            std::make_pair("auth token", GoogleServiceAuthError::NONE)));
    ExpectAccessToken(false, mojom::AccessTokenStatus::kSuccess, "auth token");
  }
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(mock_identity_accessor_,
                GetAccessToken("test@example.com", _, "drivefs"))
        .WillOnce(testing::Return(std::make_pair(
            base::nullopt, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)));
    ExpectAccessToken(false, mojom::AccessTokenStatus::kAuthError, "");
  }
}

TEST_F(DriveFsAuthTest, Caching) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  // Second attempt should reuse already available token.
  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");
}

TEST_F(DriveFsAuthTest, CachedAndNotCached) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)))
      .WillOnce(testing::Return(
          std::make_pair("auth token 2", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  // Second attempt should reuse already available token.
  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  // Now ask for token explicitly bypassing the cache.
  ExpectAccessToken(false, mojom::AccessTokenStatus::kSuccess, "auth token 2");
}

TEST_F(DriveFsAuthTest, CacheExpired) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)))
      .WillOnce(testing::Return(
          std::make_pair("auth token 2", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  clock_.Advance(base::TimeDelta::FromHours(2));

  // As the token expired second mount attempt go to identity.
  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token 2");
}

}  // namespace
}  // namespace drivefs
