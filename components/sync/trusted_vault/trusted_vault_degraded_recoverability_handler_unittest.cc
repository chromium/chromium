// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_degraded_recoverability_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {
using testing::_;
using testing::Eq;

CoreAccountInfo MakeAccountInfoWithGaiaId(const std::string& gaia_id) {
  CoreAccountInfo account_info;
  account_info.gaia = gaia_id;
  return account_info;
}

MATCHER_P(DegradedRecoverabilityStateEq, expected_state, "") {
  const sync_pb::LocalTrustedVaultDegradedRecoverabilityState& given_state =
      arg;
  return given_state.is_recoverability_degraded() ==
             expected_state.is_recoverability_degraded() &&
         given_state.last_refresh_time_millis_since_unix_epoch() ==
             expected_state.last_refresh_time_millis_since_unix_epoch();
}

class MockTrustedVaultConnection : public TrustedVaultConnection {
 public:
  MockTrustedVaultConnection() = default;
  ~MockTrustedVaultConnection() override = default;
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterAuthenticationFactor,
              (const CoreAccountInfo& account_info,
               const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
               int last_trusted_vault_key_version,
               const SecureBoxPublicKey& authentication_factor_public_key,
               AuthenticationFactorType authentication_factor_type,
               absl::optional<int> authentication_factor_type_hint,
               RegisterAuthenticationFactorCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterDeviceWithoutKeys,
              (const CoreAccountInfo& account_info,
               const SecureBoxPublicKey& device_public_key,
               RegisterDeviceWithoutKeysCallback callback),
              (override));
  MOCK_METHOD(
      std::unique_ptr<Request>,
      DownloadNewKeys,
      (const CoreAccountInfo& account_info,
       const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
       std::unique_ptr<SecureBoxKeyPair> device_key_pair,
       DownloadNewKeysCallback callback),
      (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              DownloadIsRecoverabilityDegraded,
              (const CoreAccountInfo& account_info,
               IsRecoverabilityDegradedCallback callback),
              (override));
};

class MockDelegate
    : public TrustedVaultDegradedRecoverabilityHandler::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              WriteDegradedRecoverabilityState,
              (const sync_pb::LocalTrustedVaultDegradedRecoverabilityState&),
              (override));
  MOCK_METHOD(void, OnDegradedRecoverabilityChanged, (bool), (override));
};

class TrustedVaultDegradedRecoverabilityHandlerTest : public ::testing::Test {
 public:
  TrustedVaultDegradedRecoverabilityHandlerTest() = default;
  ~TrustedVaultDegradedRecoverabilityHandlerTest() override = default;

  void SetUp() override {
    ON_CALL(connection_, DownloadIsRecoverabilityDegraded(
                             Eq(MakeAccountInfoWithGaiaId("user")), _))
        .WillByDefault(
            [&](const CoreAccountInfo&,
                MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                    callback) {
              std::move(callback).Run(
                  TrustedVaultRecoverabilityStatus::kDegraded);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    scheduler_ = std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
        &connection_, &delegate_, MakeAccountInfoWithGaiaId("user"));
    // Moving the time forward by one millisecond to make sure that the first
    // refresh had called.
    task_environment().FastForwardBy(base::Milliseconds(1));
  }

  TrustedVaultDegradedRecoverabilityHandler& scheduler() {
    return *scheduler_.get();
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  testing::NiceMock<MockTrustedVaultConnection> connection_;
  testing::NiceMock<MockDelegate> delegate_;
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshOnceWhenInitialize) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"));
  task_environment().FastForwardBy(base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshImmediately) {
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded);
  scheduler().RefreshImmediately();
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshOncePerLongPeriod) {
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(kLongDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToShortPeriod) {
  scheduler().StartShortIntervalRefreshing();
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToLongPeriod) {
  scheduler().StartShortIntervalRefreshing();
  scheduler().StartLongIntervalRefreshing();
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded).Times(0);
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(kLongDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToShortPeriodAndAccountForTimePassed) {
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod -
                                   base::Seconds(1));
  scheduler().StartShortIntervalRefreshing();
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(base::Seconds(1) + base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToShortPeriodAndRefreshImmediately) {
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod +
                                   base::Seconds(1));
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded);
  scheduler().StartShortIntervalRefreshing();
  task_environment().FastForwardBy(base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldWriteTheStateImmediatelyWithRecoverabilityDegradedAndCurrentTime) {
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_is_recoverability_degraded(true);
  // Since the time is not moving, the `Time::Now()` is the expected to be
  // written.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded(
                               Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate_,
              WriteDegradedRecoverabilityState(DegradedRecoverabilityStateEq(
                  degraded_recoverability_state)));
  scheduler().RefreshImmediately();
}

TEST_F(
    TrustedVaultDegradedRecoverabilityHandlerTest,
    ShouldWriteTheStateImmediatelyWithRecoverabilityNotDegradedAndCurrentTime) {
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_is_recoverability_degraded(false);
  // Since the time is not moving, the `Time::Now()` is the expected to be
  // written.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));
  EXPECT_CALL(connection_, DownloadIsRecoverabilityDegraded(
                               Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kNotDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate_,
              WriteDegradedRecoverabilityState(DegradedRecoverabilityStateEq(
                  degraded_recoverability_state)));
  scheduler().RefreshImmediately();
}

}  // namespace
}  // namespace syncer
