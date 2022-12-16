// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_degraded_recoverability_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
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
  return given_state.degraded_recoverability_value() ==
             expected_state.degraded_recoverability_value() &&
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
  MOCK_METHOD(void, OnDegradedRecoverabilityChanged, (), (override));
};

class TrustedVaultDegradedRecoverabilityHandlerTest : public ::testing::Test {
 public:
  TrustedVaultDegradedRecoverabilityHandlerTest() = default;
  ~TrustedVaultDegradedRecoverabilityHandlerTest() override = default;

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRecordTheDegradedRecoverabilityValueOnStart) {
  base::HistogramTester histogram_tester;
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kNotDegraded);

  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDegradedRecoverabilityValue2",
      /*sample=*/sync_pb::DegradedRecoverabilityValue::kNotDegraded,
      /*expected_bucket_count=*/0);

  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDegradedRecoverabilityValue2",
      /*sample=*/sync_pb::DegradedRecoverabilityValue::kNotDegraded,
      /*expected_bucket_count=*/1);
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldPendTheCallbackUntilTheFirstRefreshIsCalled) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          sync_pb::LocalTrustedVaultDegradedRecoverabilityState());
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(completion_callback, Run(true));
  scheduler->GetIsRecoverabilityDegraded(completion_callback.Get());
  task_environment().FastForwardBy(base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldInvokeTheCallbackImmediatelyWhenTheFirstRefreshIsAlreadyCalled) {
  // Note: The first Refresh() could already be happened on a previous handler
  // instance.
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kNotDegraded);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded).Times(0);
  EXPECT_CALL(completion_callback, Run(false));
  scheduler->GetIsRecoverabilityDegraded(completion_callback.Get());
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshImmediatelyAndRecordTheReason) {
  base::HistogramTester histogram_tester;
  testing::NiceMock<MockTrustedVaultConnection> connection;
  ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                          Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillByDefault(
          [&](const CoreAccountInfo&,
              MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                  callback) {
            std::move(callback).Run(
                TrustedVaultRecoverabilityStatus::kNotDegraded);
            return std::make_unique<TrustedVaultConnection::Request>();
          });
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          sync_pb::LocalTrustedVaultDegradedRecoverabilityState());
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  // Moving the time forward by one millisecond to make sure that the first
  // refresh had called.
  task_environment().FastForwardBy(base::Milliseconds(1));

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  scheduler->HintDegradedRecoverabilityChanged(
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
          kPersistentAuthErrorResolved);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultHintDegradedRecoverabilityChangedReason2",
      /*sample=*/
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
          kPersistentAuthErrorResolved,
      /*expected_bucket_count=*/1);
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshOncePerShortPeriod) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kDegraded);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get() +
      base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshOncePerLongPeriod) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kNotDegraded);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded).Times(0);
  task_environment().FastForwardBy(
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get() +
      base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(
      kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling.Get() -
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get());
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToShortPeriod) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          sync_pb::LocalTrustedVaultDegradedRecoverabilityState());

  // Make handler aware about degraded recoverability.
  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate, OnDegradedRecoverabilityChanged);
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  task_environment().FastForwardBy(base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  // Verify that handler switches to short polling period.
  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get() +
      base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToLongPeriod) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kDegraded);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());

  // Make handler aware about degraded recoverability.
  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kNotDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate, OnDegradedRecoverabilityChanged);
  task_environment().FastForwardBy(
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get() +
      base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  // Verify that handler switches to long polling period.

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded).Times(0);
  task_environment().FastForwardBy(
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get() +
      base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(
      kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling.Get() -
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get());
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldWriteTheStateImmediatelyWithRecoverabilityDegradedAndCurrentTime) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                          Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillByDefault(
          [&](const CoreAccountInfo&,
              MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                  callback) {
            std::move(callback).Run(
                TrustedVaultRecoverabilityStatus::kNotDegraded);
            return std::make_unique<TrustedVaultConnection::Request>();
          });
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          sync_pb::LocalTrustedVaultDegradedRecoverabilityState());
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  // Moving the time forward by one millisecond to make sure that the first
  // refresh had called.
  task_environment().FastForwardBy(base::Milliseconds(1));

  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kDegraded);
  // Since the time is not moving, the `Time::Now()` is the expected to be
  // written.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate,
              WriteDegradedRecoverabilityState(DegradedRecoverabilityStateEq(
                  degraded_recoverability_state)));
  scheduler->HintDegradedRecoverabilityChanged(
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA());
}

TEST_F(
    TrustedVaultDegradedRecoverabilityHandlerTest,
    ShouldWriteTheStateImmediatelyWithRecoverabilityNotDegradedAndCurrentTime) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                          Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillByDefault(
          [&](const CoreAccountInfo&,
              MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                  callback) {
            std::move(callback).Run(
                TrustedVaultRecoverabilityStatus::kDegraded);
            return std::make_unique<TrustedVaultConnection::Request>();
          });
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          sync_pb::LocalTrustedVaultDegradedRecoverabilityState());
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  // Moving the time forward by one millisecond to make sure that the first
  // refresh had called.
  task_environment().FastForwardBy(base::Milliseconds(1));

  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      sync_pb::DegradedRecoverabilityValue::kNotDegraded);
  // Since the time is not moving, the `Time::Now()` is the expected to be
  // written.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultConnection::IsRecoverabilityDegradedCallback
                        callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kNotDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate,
              WriteDegradedRecoverabilityState(DegradedRecoverabilityStateEq(
                  degraded_recoverability_state)));
  scheduler->HintDegradedRecoverabilityChanged(
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA());
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldComputeTheNextRefreshTimeBasedOnTheStoredState) {
  testing::NiceMock<MockTrustedVaultConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now() - base::Minutes(1)));

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  task_environment().FastForwardBy(
      kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling.Get() -
      base::Minutes(1) + base::Milliseconds(1));
}

}  // namespace
}  // namespace syncer
