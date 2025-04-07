// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_degraded_recoverability_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using testing::_;
using testing::Eq;

CoreAccountInfo MakeAccountInfoWithGaiaId(const std::string& gaia_id) {
  CoreAccountInfo account_info;
  account_info.gaia = GaiaId(gaia_id);
  return account_info;
}

MATCHER_P(DegradedRecoverabilityStateEq, expected_state, "") {
  const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
      given_state = arg;
  return given_state.degraded_recoverability_value() ==
             expected_state.degraded_recoverability_value() &&
         given_state.last_refresh_time_millis_since_unix_epoch() ==
             expected_state.last_refresh_time_millis_since_unix_epoch();
}

class MockDelegate
    : public TrustedVaultDegradedRecoverabilityHandler::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(
      void,
      WriteDegradedRecoverabilityState,
      (const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&),
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

  base::TimeDelta short_refresh_period() const {
    return TrustedVaultDegradedRecoverabilityHandler::
        kShortDegradedRecoverabilityRefreshPeriod;
  }

  base::TimeDelta long_refresh_period() const{
    return TrustedVaultDegradedRecoverabilityHandler::
        kLongDegradedRecoverabilityRefreshPeriod;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRecordTheDegradedRecoverabilityValueOnStart) {
  base::HistogramTester histogram_tester;
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded);

  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          degraded_recoverability_state);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.TrustedVaultDegradedRecoverabilityValue",
      /*sample=*/trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded,
      /*expected_bucket_count=*/0);

  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.TrustedVaultDegradedRecoverabilityValue",
      /*sample=*/trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded,
      /*expected_bucket_count=*/1);
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldPendTheCallbackUntilTheFirstRefreshIsCalled) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState());
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultThrottlingConnection::
                        IsRecoverabilityDegradedCallback callback) {
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
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded);
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
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                          Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillByDefault([&](const CoreAccountInfo&,
                         MockTrustedVaultThrottlingConnection::
                             IsRecoverabilityDegradedCallback callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kNotDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState());
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
      "TrustedVault.TrustedVaultHintDegradedRecoverabilityChangedReason",
      /*sample=*/
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
          kPersistentAuthErrorResolved,
      /*expected_bucket_count=*/1);
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshOncePerShortPeriod) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
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
      short_refresh_period() +
      base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRefreshOncePerLongPeriod) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded);
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
      short_refresh_period() +
      base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(
      long_refresh_period() - short_refresh_period());
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToShortPeriod) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState());

  // Make handler aware about degraded recoverability.
  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultThrottlingConnection::
                        IsRecoverabilityDegradedCallback callback) {
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
      short_refresh_period() +
      base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldSwitchToLongPeriod) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
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
                    MockTrustedVaultThrottlingConnection::
                        IsRecoverabilityDegradedCallback callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kNotDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  EXPECT_CALL(delegate, OnDegradedRecoverabilityChanged);
  task_environment().FastForwardBy(
      short_refresh_period() +
      base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  // Verify that handler switches to long polling period.

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded).Times(0);
  task_environment().FastForwardBy(
      short_refresh_period() +
      base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&connection);

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded);
  task_environment().FastForwardBy(
      long_refresh_period() -
      short_refresh_period());
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldWriteTheStateImmediatelyWithRecoverabilityDegradedAndCurrentTime) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                          Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillByDefault([&](const CoreAccountInfo&,
                         MockTrustedVaultThrottlingConnection::
                             IsRecoverabilityDegradedCallback callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kNotDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState());
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  // Moving the time forward by one millisecond to make sure that the first
  // refresh had called.
  task_environment().FastForwardBy(base::Milliseconds(1));

  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
  // Since the time is not moving, the `Time::Now()` is the expected to be
  // written.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultThrottlingConnection::
                        IsRecoverabilityDegradedCallback callback) {
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
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                          Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillByDefault([&](const CoreAccountInfo&,
                         MockTrustedVaultThrottlingConnection::
                             IsRecoverabilityDegradedCallback callback) {
        std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  testing::NiceMock<MockDelegate> delegate;

  // Passing empty LocalDegradedRecoverability state indicates that this is the
  // first initialization and new state needs to be fetched immediately.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler> scheduler =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
          trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState());
  // Start the scheduler.
  scheduler->GetIsRecoverabilityDegraded(base::DoNothing());
  // Moving the time forward by one millisecond to make sure that the first
  // refresh had called.
  task_environment().FastForwardBy(base::Milliseconds(1));

  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded);
  // Since the time is not moving, the `Time::Now()` is the expected to be
  // written.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));

  EXPECT_CALL(connection, DownloadIsRecoverabilityDegraded(
                              Eq(MakeAccountInfoWithGaiaId("user")), _))
      .WillOnce([&](const CoreAccountInfo&,
                    MockTrustedVaultThrottlingConnection::
                        IsRecoverabilityDegradedCallback callback) {
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
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
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
      long_refresh_period() -
      base::Minutes(1) + base::Milliseconds(1));
}

TEST_F(TrustedVaultDegradedRecoverabilityHandlerTest,
       ShouldRecordDegradedRecoverabilityStatusOnRequestCompletion) {
  testing::NiceMock<MockTrustedVaultThrottlingConnection> connection;
  testing::NiceMock<MockDelegate> delegate;

  // Start the handler, this will trigger the first request.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler>
      degraded_recoverability_handler =
          std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
              &connection, &delegate, MakeAccountInfoWithGaiaId("user"),
              trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState());
  {
    base::RunLoop run_loop;
    ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                            Eq(MakeAccountInfoWithGaiaId("user")), _))
        .WillByDefault([&](const CoreAccountInfo&,
                           MockTrustedVaultThrottlingConnection::
                               IsRecoverabilityDegradedCallback callback) {
          std::move(callback).Run(
              TrustedVaultRecoverabilityStatus::kNotDegraded);
          run_loop.Quit();
          return std::make_unique<TrustedVaultConnection::Request>();
        });

    base::HistogramTester histogram_tester;
    // This will start the handler and trigger the first request.
    degraded_recoverability_handler->GetIsRecoverabilityDegraded(
        base::DoNothing());
    run_loop.Run();
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.RecoverabilityStatusOnRequestCompletion",
        /*sample=*/
        TrustedVaultRecoverabilityStatus::kNotDegraded,
        /*expected_bucket_count=*/1);
  }

  {
    base::RunLoop run_loop;
    ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                            Eq(MakeAccountInfoWithGaiaId("user")), _))
        .WillByDefault([&](const CoreAccountInfo&,
                           MockTrustedVaultThrottlingConnection::
                               IsRecoverabilityDegradedCallback callback) {
          std::move(callback).Run(TrustedVaultRecoverabilityStatus::kDegraded);
          run_loop.Quit();
          return std::make_unique<TrustedVaultConnection::Request>();
        });

    base::HistogramTester histogram_tester;
    // This will force a request.
    degraded_recoverability_handler->HintDegradedRecoverabilityChanged(
        TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA());
    run_loop.Run();
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.RecoverabilityStatusOnRequestCompletion",
        /*sample=*/
        TrustedVaultRecoverabilityStatus::kDegraded,
        /*expected_bucket_count=*/1);
  }

  {
    base::RunLoop run_loop;
    ON_CALL(connection, DownloadIsRecoverabilityDegraded(
                            Eq(MakeAccountInfoWithGaiaId("user")), _))
        .WillByDefault([&](const CoreAccountInfo&,
                           MockTrustedVaultThrottlingConnection::
                               IsRecoverabilityDegradedCallback callback) {
          std::move(callback).Run(TrustedVaultRecoverabilityStatus::kError);
          run_loop.Quit();
          return std::make_unique<TrustedVaultConnection::Request>();
        });

    base::HistogramTester histogram_tester;
    // This will force a request.
    degraded_recoverability_handler->HintDegradedRecoverabilityChanged(
        TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA());
    run_loop.Run();
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.RecoverabilityStatusOnRequestCompletion",
        /*sample=*/
        TrustedVaultRecoverabilityStatus::kError,
        /*expected_bucket_count=*/1);
  }
}

}  // namespace
}  // namespace trusted_vault
