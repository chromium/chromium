// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_throttling_connection_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/simple_test_clock.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/test/fake_file_access.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;

class TrustedVaultThrottlingConnectionImplTest : public testing::Test {
 public:
  TrustedVaultThrottlingConnectionImplTest() {
    clock_.SetNow(base::Time::Now());
    ResetThrottlingConnection();
  }

  void ResetThrottlingConnection() {
    // Destroy `throttling_connection_`, otherwise it would hold a reference to
    // `storage_` which is destroyed before `throttling_connection_` below.
    // Also, set `delegate_` to null, because it points to an object owned by
    // `throttling_connection_`.
    delegate_ = nullptr;
    throttling_connection_ = nullptr;

    std::unique_ptr<FakeFileAccess> file_access =
        std::make_unique<FakeFileAccess>();
    if (file_access_) {
      // Retain the stored state.
      file_access->SetStoredLocalTrustedVault(
          file_access_->GetStoredLocalTrustedVault());
    }
    file_access_ = file_access.get();
    storage_ =
        StandaloneTrustedVaultStorage::CreateForTesting(std::move(file_access));
    storage_->ReadDataFromDisk();
    if (storage_->FindUserVault(account_info().gaia) == nullptr) {
      storage_->AddUserVault(account_info().gaia);
    }

    std::unique_ptr<NiceMock<MockTrustedVaultThrottlingConnection>> delegate =
        std::make_unique<NiceMock<MockTrustedVaultThrottlingConnection>>();
    delegate_ = delegate.get();

    throttling_connection_ =
        TrustedVaultThrottlingConnectionImpl::CreateForTesting(
            std::move(delegate), storage_.get(), &clock_);
  }

  ~TrustedVaultThrottlingConnectionImplTest() override = default;

  MockTrustedVaultThrottlingConnection* delegate() { return delegate_; }

  base::SimpleTestClock* clock() { return &clock_; }

  TrustedVaultThrottlingConnectionImpl* throttling_connection() {
    return throttling_connection_.get();
  }

  CoreAccountInfo account_info() {
    CoreAccountInfo account_info;
    account_info.gaia = GaiaId("user");
    return account_info;
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<StandaloneTrustedVaultStorage> storage_;
  std::unique_ptr<TrustedVaultThrottlingConnectionImpl> throttling_connection_;
  raw_ptr<NiceMock<MockTrustedVaultThrottlingConnection>> delegate_ = nullptr;
  raw_ptr<FakeFileAccess> file_access_ = nullptr;
};

TEST_F(TrustedVaultThrottlingConnectionImplTest, ShouldNotThrottleByDefault) {
  EXPECT_FALSE(throttling_connection()->AreRequestsThrottled(account_info()));
}

TEST_F(TrustedVaultThrottlingConnectionImplTest, FailedAttemptShouldThrottle) {
  EXPECT_FALSE(throttling_connection()->AreRequestsThrottled(account_info()));

  throttling_connection()->RecordFailedRequestForThrottling(account_info());

  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));
}

TEST_F(TrustedVaultThrottlingConnectionImplTest, ShouldRemainThrottled) {
  // Record a failed attempt at time "now".
  throttling_connection()->RecordFailedRequestForThrottling(account_info());
  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));

  // Advance time to just before the throttling duration.
  clock()->Advance(TrustedVaultThrottlingConnectionImpl::kThrottlingDuration -
                   base::Seconds(1));
  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));
}

TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldUnthrottleAfterThrottlingDuration) {
  // Record a failed attempt at time "now".
  throttling_connection()->RecordFailedRequestForThrottling(account_info());
  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));

  // Advance time to pass the throttling duration.
  clock()->Advance(TrustedVaultThrottlingConnectionImpl::kThrottlingDuration);
  EXPECT_FALSE(throttling_connection()->AreRequestsThrottled(account_info()));
}

// System time can be changed to the past and if this situation not handled,
// requests could be throttled for unreasonable amount of time.
TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldUnthrottleWhenTimeSetToPast) {
  // Record a failed attempt at time "now".
  throttling_connection()->RecordFailedRequestForThrottling(account_info());
  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));

  // Mimic system set to the past, which should unthrottle automatically.
  clock()->Advance(base::Seconds(-1));
  EXPECT_FALSE(throttling_connection()->AreRequestsThrottled(account_info()));
}

TEST_F(TrustedVaultThrottlingConnectionImplTest, ShouldRestoreThrottlingState) {
  // Record a failed attempt at time "now".
  throttling_connection()->RecordFailedRequestForThrottling(account_info());
  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));

  // Reset the connection, which restores the previously stored state.
  ResetThrottlingConnection();

  EXPECT_TRUE(throttling_connection()->AreRequestsThrottled(account_info()));
}

TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldCallRegisterAuthenticationFactor) {
  EXPECT_CALL(*delegate(), RegisterAuthenticationFactor)
      .WillOnce(InvokeWithoutArgs([]() {
        return std::make_unique<TrustedVaultConnection::Request>();
      }));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      throttling_connection()->RegisterAuthenticationFactor(
          account_info(),
          GetTrustedVaultKeysWithVersions(std::vector<std::vector<uint8_t>>(),
                                          0),
          SecureBoxKeyPair::GenerateRandom()->public_key(),
          LocalPhysicalDevice(), base::DoNothing());
  EXPECT_THAT(request, NotNull());
}

TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldCallRegisterLocalDeviceWithoutKeys) {
  EXPECT_CALL(*delegate(), RegisterLocalDeviceWithoutKeys)
      .WillOnce(InvokeWithoutArgs([]() {
        return std::make_unique<TrustedVaultConnection::Request>();
      }));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      throttling_connection()->RegisterLocalDeviceWithoutKeys(
          account_info(), SecureBoxKeyPair::GenerateRandom()->public_key(),
          base::DoNothing());
  EXPECT_THAT(request, NotNull());
}

TEST_F(TrustedVaultThrottlingConnectionImplTest, ShouldCallDownloadNewKeys) {
  EXPECT_CALL(*delegate(), DownloadNewKeys).WillOnce(InvokeWithoutArgs([]() {
    return std::make_unique<TrustedVaultConnection::Request>();
  }));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      throttling_connection()->DownloadNewKeys(
          account_info(), TrustedVaultKeyAndVersion(std::vector<uint8_t>(), 0),
          SecureBoxKeyPair::GenerateRandom(), base::DoNothing());
  EXPECT_THAT(request, NotNull());
}

TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldCallDownloadIsRecoverabilityDegraded) {
  EXPECT_CALL(*delegate(), DownloadIsRecoverabilityDegraded)
      .WillOnce(InvokeWithoutArgs([]() {
        return std::make_unique<TrustedVaultConnection::Request>();
      }));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      throttling_connection()->DownloadIsRecoverabilityDegraded(
          account_info(), base::DoNothing());
  EXPECT_THAT(request, NotNull());
}

TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldCallDownloadAuthenticationFactorsRegistrationState) {
  EXPECT_CALL(*delegate(),
              DownloadAuthenticationFactorsRegistrationState(_, _, _))
      .WillOnce(InvokeWithoutArgs([]() {
        return std::make_unique<TrustedVaultConnection::Request>();
      }));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      throttling_connection()->DownloadAuthenticationFactorsRegistrationState(
          account_info(), base::DoNothing(), base::DoNothing());
  EXPECT_THAT(request, NotNull());
}

TEST_F(TrustedVaultThrottlingConnectionImplTest,
       ShouldCallDownloadAuthenticationFactorsRegistrationStateWithFilter) {
  EXPECT_CALL(*delegate(),
              DownloadAuthenticationFactorsRegistrationState(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([]() {
        return std::make_unique<TrustedVaultConnection::Request>();
      }));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      throttling_connection()->DownloadAuthenticationFactorsRegistrationState(
          account_info(),
          {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE},
          base::DoNothing(), base::DoNothing());
  EXPECT_THAT(request, NotNull());
}

}  // namespace

}  // namespace trusted_vault
