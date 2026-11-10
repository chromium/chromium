// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_gateway.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/text_log_receiver.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Test;

class RecordingLogReceiver : public LogReceiver {
 public:
  void LogEntry(const base::DictValue& entry) override {
    entries_.push_back(TextLogReceiver().LogEntryToText(entry));
  }

  const std::vector<std::string>& entries() const { return entries_; }

 private:
  std::vector<std::string> entries_;
};

class MockOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  MOCK_METHOD(void,
              StartOtpPhishGuardCheck,
              (LocalFrameToken, base::OnceCallback<void(bool is_phishing)>),
              (override));
};

class OtpPhishGuardGatewayTest : public Test {
 public:
  void SetUp() override {
    client_.log_router().RegisterReceiver(&log_receiver_);
    auto delegate = std::make_unique<MockOtpPhishGuardDelegate>();
    phish_guard_delegate_ = delegate.get();
    client_.set_otp_phish_guard_delegate(std::move(delegate));
  }

  void TearDown() override {
    phish_guard_delegate_ = nullptr;
    client_.log_router().UnregisterReceiver(&log_receiver_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  RecordingLogReceiver log_receiver_;
  raw_ptr<MockOtpPhishGuardDelegate> phish_guard_delegate_ = nullptr;
};

// Verifies that `Check` invokes `OtpPhishGuardDelegate`, maps `false` to
// `kNotPhishing`, and records `CheckPerformed`, `Latency`, and `Verdict`
// histograms.
TEST_F(OtpPhishGuardGatewayTest,
       Check_NotPhishingVerdictRecordsMetricsAndInvokesCallback) {
  base::HistogramTester histogram_tester;
  OtpPhishGuardGateway gateway(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();

  base::OnceCallback<void(bool)> delegate_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        delegate_cb = std::move(cb);
      });

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> verdict_future;
  gateway.Check(frame, verdict_future.GetCallback());
  ASSERT_TRUE(delegate_cb);
  EXPECT_FALSE(verdict_future.IsReady());

  task_environment_.FastForwardBy(base::Milliseconds(120));
  std::move(delegate_cb).Run(/*is_phishing=*/false);

  EXPECT_EQ(verdict_future.Get(), OneTimeTokensPhishGuardVerdict::kNotPhishing);
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      OtpPhishGuardGateway::kLatencyHistogram, base::Milliseconds(120), 1);
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kVerdictHistogram,
      OneTimeTokensPhishGuardVerdict::kNotPhishing, 1);
  EXPECT_THAT(log_receiver_.entries(), Contains(HasSubstr("NotPhishing")));
}

// Verifies that `Check` maps `is_phishing = true` to `kPhishing` and records
// PhishGuard histograms.
TEST_F(OtpPhishGuardGatewayTest,
       Check_PhishingVerdictRecordsMetricsAndInvokesCallback) {
  base::HistogramTester histogram_tester;
  OtpPhishGuardGateway gateway(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();

  base::OnceCallback<void(bool)> delegate_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        delegate_cb = std::move(cb);
      });

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> verdict_future;
  gateway.Check(frame, verdict_future.GetCallback());
  ASSERT_TRUE(delegate_cb);

  task_environment_.FastForwardBy(base::Milliseconds(50));
  std::move(delegate_cb).Run(/*is_phishing=*/true);

  EXPECT_EQ(verdict_future.Get(), OneTimeTokensPhishGuardVerdict::kPhishing);
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      OtpPhishGuardGateway::kLatencyHistogram, base::Milliseconds(50), 1);
  histogram_tester.ExpectUniqueSample(OtpPhishGuardGateway::kVerdictHistogram,
                                      OneTimeTokensPhishGuardVerdict::kPhishing,
                                      1);
}

// Verifies that `Check` does not cache verdicts by `LocalFrameToken`, ensuring
// same-`RenderFrameHost` navigations (e.g. SPA URL changes) re-query
// `OtpPhishGuardDelegate` rather than reusing a stale safe verdict.
TEST_F(OtpPhishGuardGatewayTest,
       Check_DoesNotCacheVerdictAcrossRepeatCallsForSameFrame) {
  base::HistogramTester histogram_tester;
  OtpPhishGuardGateway gateway(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/true));

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> first_check;
  gateway.Check(frame, first_check.GetCallback());
  EXPECT_EQ(first_check.Get(), OneTimeTokensPhishGuardVerdict::kNotPhishing);

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> second_check;
  gateway.Check(frame, second_check.GetCallback());
  EXPECT_EQ(second_check.Get(), OneTimeTokensPhishGuardVerdict::kPhishing);

  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 2);
  histogram_tester.ExpectBucketCount(
      OtpPhishGuardGateway::kVerdictHistogram,
      OneTimeTokensPhishGuardVerdict::kNotPhishing, 1);
  histogram_tester.ExpectBucketCount(OtpPhishGuardGateway::kVerdictHistogram,
                                     OneTimeTokensPhishGuardVerdict::kPhishing,
                                     1);
}

// Verifies that overlapping `Check` calls are serialized in FIFO order so a
// single-flight `OtpPhishGuardDelegate` never clobbers an in-flight check, and
// queue wait time is excluded from `Latency`.
TEST_F(OtpPhishGuardGatewayTest,
       Check_SerializesConcurrentChecksAndExcludesQueueWaitFromLatency) {
  base::HistogramTester histogram_tester;
  OtpPhishGuardGateway gateway(client_);
  const LocalFrameToken frame_1 = test::MakeLocalFrameToken();
  const LocalFrameToken frame_2 = test::MakeLocalFrameToken();

  base::OnceCallback<void(bool)> delegate_cb_1;
  base::OnceCallback<void(bool)> delegate_cb_2;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_1, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        delegate_cb_1 = std::move(cb);
      });

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> first_future;
  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> second_future;
  gateway.Check(frame_1, first_future.GetCallback());
  gateway.Check(frame_2, second_future.GetCallback());

  // `frame_2` remains queued until `frame_1` completes.
  ASSERT_TRUE(delegate_cb_1);
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(second_future.IsReady());
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 1);

  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_2, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        delegate_cb_2 = std::move(cb);
      });

  task_environment_.FastForwardBy(base::Milliseconds(40));
  std::move(delegate_cb_1).Run(/*is_phishing=*/false);

  EXPECT_EQ(first_future.Get(), OneTimeTokensPhishGuardVerdict::kNotPhishing);
  ASSERT_TRUE(delegate_cb_2);
  EXPECT_FALSE(second_future.IsReady());

  task_environment_.FastForwardBy(base::Milliseconds(75));
  std::move(delegate_cb_2).Run(/*is_phishing=*/true);

  EXPECT_EQ(second_future.Get(), OneTimeTokensPhishGuardVerdict::kPhishing);
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 2);
  histogram_tester.ExpectBucketCount(OtpPhishGuardGateway::kLatencyHistogram,
                                     40, 1);
  histogram_tester.ExpectBucketCount(OtpPhishGuardGateway::kLatencyHistogram,
                                     75, 1);
  histogram_tester.ExpectBucketCount(
      OtpPhishGuardGateway::kVerdictHistogram,
      OneTimeTokensPhishGuardVerdict::kNotPhishing, 1);
  histogram_tester.ExpectBucketCount(OtpPhishGuardGateway::kVerdictHistogram,
                                     OneTimeTokensPhishGuardVerdict::kPhishing,
                                     1);
}

// Verifies that reentrant `Check()` calls and gateway destruction from inside
// `VerdictCallback` are handled safely without use-after-free or double
// dispatch.
TEST_F(OtpPhishGuardGatewayTest,
       Check_ReentrantCheckAndDestroyInsideCallbackHandledSafely) {
  auto gateway = std::make_unique<OtpPhishGuardGateway>(client_);
  const LocalFrameToken frame_1 = test::MakeLocalFrameToken();
  const LocalFrameToken frame_2 = test::MakeLocalFrameToken();
  const LocalFrameToken frame_3 = test::MakeLocalFrameToken();

  base::OnceCallback<void(bool)> delegate_cb_1;
  base::OnceCallback<void(bool)> delegate_cb_2;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_1, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        delegate_cb_1 = std::move(cb);
      });
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_2, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        delegate_cb_2 = std::move(cb);
      });
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_3, _))
      .Times(0);

  base::MockCallback<OtpPhishGuardGateway::VerdictCallback> third_callback;
  EXPECT_CALL(third_callback, Run).Times(0);

  gateway->Check(
      frame_1,
      base::BindLambdaForTesting([&](OneTimeTokensPhishGuardVerdict verdict) {
        EXPECT_EQ(verdict, OneTimeTokensPhishGuardVerdict::kNotPhishing);
        gateway->Check(frame_3, third_callback.Get());
      }));
  gateway->Check(
      frame_2,
      base::BindLambdaForTesting([&](OneTimeTokensPhishGuardVerdict verdict) {
        EXPECT_EQ(verdict, OneTimeTokensPhishGuardVerdict::kNotPhishing);
        gateway.reset();
      }));

  ASSERT_TRUE(delegate_cb_1);
  std::move(delegate_cb_1).Run(/*is_phishing=*/false);
  ASSERT_TRUE(delegate_cb_2);
  std::move(delegate_cb_2).Run(/*is_phishing=*/false);
  EXPECT_EQ(gateway, nullptr);
}

// Verifies that `Check` returns `kUnknown` and skips the latency histogram when
// no `OtpPhishGuardDelegate` is attached.
TEST_F(OtpPhishGuardGatewayTest,
       Check_WithoutDelegateReturnsUnknownAndSkipsLatencyMetric) {
  base::HistogramTester histogram_tester;
  phish_guard_delegate_ = nullptr;
  client_.set_otp_phish_guard_delegate(nullptr);

  OtpPhishGuardGateway gateway(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> unknown_check;
  gateway.Check(frame, unknown_check.GetCallback());
  EXPECT_EQ(unknown_check.Get(), OneTimeTokensPhishGuardVerdict::kUnknown);
  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, false, 1);
  histogram_tester.ExpectTotalCount(OtpPhishGuardGateway::kLatencyHistogram, 0);
  histogram_tester.ExpectUniqueSample(OtpPhishGuardGateway::kVerdictHistogram,
                                      OneTimeTokensPhishGuardVerdict::kUnknown,
                                      1);
}

// Verifies that destroying `OtpPhishGuardGateway` while a delegate check is in
// flight safely cancels completion via `WeakPtr` without invoking `callback` or
// recording completion metrics.
TEST_F(OtpPhishGuardGatewayTest,
       Check_GatewayDestroyedWhileCheckInFlightCancelsCleanly) {
  base::HistogramTester histogram_tester;
  auto gateway = std::make_unique<OtpPhishGuardGateway>(client_);
  const LocalFrameToken frame = test::MakeLocalFrameToken();

  base::OnceCallback<void(bool)> in_flight_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        in_flight_cb = std::move(cb);
      });

  base::MockCallback<OtpPhishGuardGateway::VerdictCallback> verdict_callback;
  EXPECT_CALL(verdict_callback, Run).Times(0);

  gateway->Check(frame, verdict_callback.Get());
  ASSERT_TRUE(in_flight_cb);

  gateway.reset();
  std::move(in_flight_cb).Run(/*is_phishing=*/false);

  histogram_tester.ExpectUniqueSample(
      OtpPhishGuardGateway::kCheckPerformedHistogram, true, 1);
  histogram_tester.ExpectTotalCount(OtpPhishGuardGateway::kLatencyHistogram, 0);
  histogram_tester.ExpectTotalCount(OtpPhishGuardGateway::kVerdictHistogram, 0);
}

// Verifies that if `OtpPhishGuardDelegate` drops its callback (e.g. network
// failure or timeout inside `PasswordProtectionService`),
// `OtpPhishGuardGateway` times out after `kCheckTimeout`, completes the check,
// and drains subsequent queued checks instead of stalling the tab queue.
TEST_F(OtpPhishGuardGatewayTest,
       Check_DroppedCallbackTimesOutAndDrainsNextCheck) {
  base::HistogramTester histogram_tester;
  OtpPhishGuardGateway gateway(client_);
  const LocalFrameToken frame_1 = test::MakeLocalFrameToken();
  const LocalFrameToken frame_2 = test::MakeLocalFrameToken();

  base::OnceCallback<void(bool)> late_first_cb;
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_1, _))
      .WillOnce([&](LocalFrameToken, base::OnceCallback<void(bool)> cb) {
        late_first_cb = std::move(cb);
      });
  EXPECT_CALL(*phish_guard_delegate_, StartOtpPhishGuardCheck(frame_2, _))
      .WillOnce(RunOnceCallback<1>(/*is_phishing=*/false));

  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> first_future;
  base::test::TestFuture<OneTimeTokensPhishGuardVerdict> second_future;
  gateway.Check(frame_1, first_future.GetCallback());
  gateway.Check(frame_2, second_future.GetCallback());
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(second_future.IsReady());

  task_environment_.FastForwardBy(OtpPhishGuardGateway::kCheckTimeout);
  EXPECT_EQ(first_future.Get(), OneTimeTokensPhishGuardVerdict::kTimedOut);
  EXPECT_EQ(second_future.Get(), OneTimeTokensPhishGuardVerdict::kNotPhishing);
  histogram_tester.ExpectUniqueTimeSample(
      OtpPhishGuardGateway::kLatencyHistogram, base::Milliseconds(0), 1);
  histogram_tester.ExpectBucketCount(OtpPhishGuardGateway::kVerdictHistogram,
                                     OneTimeTokensPhishGuardVerdict::kTimedOut,
                                     1);
  histogram_tester.ExpectBucketCount(
      OtpPhishGuardGateway::kVerdictHistogram,
      OneTimeTokensPhishGuardVerdict::kNotPhishing, 1);

  // If the first check's callback is invoked after timing out, it is ignored.
  ASSERT_TRUE(late_first_cb);
  std::move(late_first_cb).Run(/*is_phishing=*/true);
}

}  // namespace
}  // namespace autofill
