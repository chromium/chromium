// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl_test_base.h>
#include <cmath>
#include <string>
#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/fuchsia_legacymetrics/legacymetrics_client.h"
#include "components/fuchsia_legacymetrics/legacymetrics_histogram_flattener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia_legacymetrics {
namespace {

using ::testing::Property;
using ::testing::UnorderedElementsAreArray;

constexpr base::TimeDelta kReportInterval = base::Minutes(1);
constexpr base::TimeDelta kShortDuration = base::Seconds(1);

class TestMetricsRecorder
    : public fuchsia::legacymetrics::testing::MetricsRecorder_TestBase {
 public:
  TestMetricsRecorder() = default;
  ~TestMetricsRecorder() override = default;

  bool IsRecordInFlight() const { return ack_callback_.has_value(); }

  bool IsEmpty() const { return recorded_events_.empty(); }

  std::vector<fuchsia::legacymetrics::Event> WaitForEvents() {
    if (recorded_events_.empty()) {
      base::RunLoop run_loop;
      on_record_cb_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return std::move(recorded_events_);
  }

  void DropAck() { ack_callback_ = std::nullopt; }

  void SendAck() {
    (*ack_callback_)();
    ack_callback_ = std::nullopt;
  }

  void set_expect_ack_dropped(bool expect_dropped) {
    expect_ack_dropped_ = expect_dropped;
  }

  // fuchsia::legacymetrics::MetricsRecorder implementation.
  void Record(std::vector<fuchsia::legacymetrics::Event> events,
              RecordCallback callback) override {
    std::move(events.begin(), events.end(),
              std::back_inserter(recorded_events_));

    // Received a call to Record() before the previous one was acknowledged,
    // which can happen in some cases (e.g. flushing).
    if (ack_callback_)
      EXPECT_TRUE(expect_ack_dropped_);

    ack_callback_ = std::move(callback);

    if (on_record_cb_)
      std::move(on_record_cb_).Run();
  }

  void NotImplemented_(const std::string& name) override { FAIL() << name; }

 private:
  std::vector<fuchsia::legacymetrics::Event> recorded_events_;
  base::OnceClosure on_record_cb_;
  std::optional<RecordCallback> ack_callback_;
  bool expect_ack_dropped_ = false;
};

class LegacyMetricsClientTest : public testing::Test {
 public:
  LegacyMetricsClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::IO) {}
  ~LegacyMetricsClientTest() override = default;

  void SetUp() override {
    service_binding_ = MakeServiceBinding();
    base::SetRecordActionTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Flush any dirty histograms from previous test runs in this process.
    GetLegacyMetricsDeltas();
  }

  std::unique_ptr<base::ScopedSingleClientServiceBinding<
      fuchsia::legacymetrics::MetricsRecorder>>
  MakeServiceBinding() {
    return std::make_unique<base::ScopedSingleClientServiceBinding<
        fuchsia::legacymetrics::MetricsRecorder>>(
        test_context_.additional_services(), &test_recorder_);
  }

  void StartClientAndExpectConnection() {
    client_.Start(kReportInterval);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(service_binding_->has_clients());
  }

  // Disconnects the service side of the metrics FIDL channel and replaces the
  // binding with a new instance.
  void DisconnectAndRestartMetricsService() {
    service_binding_.reset();
    service_binding_ = MakeServiceBinding();
    base::RunLoop().RunUntilIdle();
  }

  void ExpectReconnectAfterDelay(const base::TimeDelta& delay) {
    // Just before the expected delay, the client shouldn't reconnect yet.
    task_environment_.FastForwardBy(delay - kShortDuration);
    EXPECT_FALSE(service_binding_->has_clients())
        << "Expected delay: " << delay;

    // Complete the full expected reconnect delay. Client should reconnect.
    task_environment_.FastForwardBy(kShortDuration);
    EXPECT_TRUE(service_binding_->has_clients()) << "Expected delay: " << delay;
  }

  void SetMetricsRecorder() {
    fidl::InterfaceHandle<fuchsia::legacymetrics::MetricsRecorder>
        metrics_recorder;
    direct_binding_.Bind(metrics_recorder.NewRequest());
    client_.SetMetricsRecorder(std::move(metrics_recorder));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::TestComponentContextForProcess test_context_;
  TestMetricsRecorder test_recorder_;
  std::unique_ptr<base::ScopedSingleClientServiceBinding<
      fuchsia::legacymetrics::MetricsRecorder>>
      service_binding_;
  fidl::Binding<fuchsia::legacymetrics::MetricsRecorder> direct_binding_{
      &test_recorder_};

  LegacyMetricsClient client_;
};

TEST_F(LegacyMetricsClientTest, ReportIntervalBoundary) {
  client_.Start(kReportInterval);

  task_environment_.FastForwardBy(kReportInterval - base::Seconds(1));
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
}

void PopulateAdditionalEvents(
    base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>
        callback) {
  fuchsia::legacymetrics::ImplementationDefinedEvent impl_event;
  impl_event.set_name("baz");

  fuchsia::legacymetrics::Event event;
  event.set_impl_defined_event(std::move(impl_event));

  std::vector<fuchsia::legacymetrics::Event> events;
  events.push_back(std::move(event));
  std::move(callback).Run(std::move(events));
}

TEST_F(LegacyMetricsClientTest, AllTypes) {
  client_.SetReportAdditionalMetricsCallback(
      base::BindRepeating(&PopulateAdditionalEvents));
  client_.Start(kReportInterval);

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ("baz", events[0].impl_defined_event().name());
  EXPECT_EQ("foo", events[1].histogram().name());
  EXPECT_EQ("bar", events[2].user_action_event().name());
}

TEST_F(LegacyMetricsClientTest, DisconnectWhileCollectingAdditionalEvents) {
  // Hold the completion callback for later execution.
  base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>
      on_report_done;
  client_.SetReportAdditionalMetricsCallback(base::BindRepeating(
      [](base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>*
             stored_on_report_done,
         base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>
             on_report_done) {
        *stored_on_report_done = std::move(on_report_done);
      },
      base::Unretained(&on_report_done)));

  client_.Start(kReportInterval);

  task_environment_.FastForwardBy(kReportInterval);

  // Disconnect the service.
  service_binding_.reset();
  base::RunLoop().RunUntilIdle();

  // Fulfill the report additional metrics callback.
  std::move(on_report_done).Run({});
}

TEST_F(LegacyMetricsClientTest, ReportSkippedNoEvents) {
  client_.Start(kReportInterval);

  // Verify that Record() is not invoked if there is no data to report.
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  // Add some events and allow the interval to lapse. Verify that the data is
  // reported.
  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();

  // Verify that Record() is skipped again for no-data.
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, MultipleReports) {
  client_.Start(kReportInterval);

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();
}

TEST_F(LegacyMetricsClientTest, NoReportIfNeverAcked) {
  client_.Start(kReportInterval);

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.DropAck();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, ReconnectAfterServiceDisconnect) {
  StartClientAndExpectConnection();
  DisconnectAndRestartMetricsService();
  EXPECT_FALSE(service_binding_->has_clients());
  task_environment_.FastForwardBy(LegacyMetricsClient::kInitialReconnectDelay);
  EXPECT_TRUE(service_binding_->has_clients());

  base::RecordComputedAction("foo");
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, ServiceDisconnectWhileRecordPending) {
  StartClientAndExpectConnection();

  base::RecordComputedAction("foo");
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  DisconnectAndRestartMetricsService();
  EXPECT_FALSE(service_binding_->has_clients());
  test_recorder_.DropAck();

  task_environment_.FastForwardBy(LegacyMetricsClient::kInitialReconnectDelay);
  EXPECT_TRUE(service_binding_->has_clients());

  base::RecordComputedAction("foo");
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, ServiceDisconnectWhileFlushing) {
  StartClientAndExpectConnection();

  base::RecordComputedAction("foo");
  client_.FlushAndDisconnect(base::OnceClosure());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  DisconnectAndRestartMetricsService();
  test_recorder_.DropAck();
  EXPECT_FALSE(service_binding_->has_clients());

  task_environment_.FastForwardBy(LegacyMetricsClient::kInitialReconnectDelay);
  EXPECT_TRUE(service_binding_->has_clients());

  base::RecordComputedAction("foo");
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest,
       ReconnectConsecutivelyWithoutRecordBacksOffExponentially) {
  StartClientAndExpectConnection();

  for (base::TimeDelta expected_delay =
           LegacyMetricsClient::kInitialReconnectDelay;
       expected_delay <= LegacyMetricsClient::kMaxReconnectDelay;
       expected_delay *= LegacyMetricsClient::kReconnectBackoffFactor) {
    DisconnectAndRestartMetricsService();
    ExpectReconnectAfterDelay(expected_delay);
  }
}

// The test is flaky.
// TODO: crbug.com/326659366 - Reenable the test.
TEST_F(LegacyMetricsClientTest, DISABLED_ReconnectDelayNeverExceedsMax) {
  StartClientAndExpectConnection();

  // Find the theoretical maximum number of consecutive failed connections. Also
  // add a few extra iterations to ensure that we never exceed the max delay.
  const size_t num_iterations =
      3 + log(LegacyMetricsClient::kMaxReconnectDelay /
              LegacyMetricsClient::kInitialReconnectDelay) /
              log(LegacyMetricsClient::kReconnectBackoffFactor);

  // As a heuristic, starting with 1 second and a factor of 2 reaches 24 hours
  // in about 17 iterations. So the expected number of iterations needed to
  // reach the maximum delay should be less than about 20.
  EXPECT_LE(num_iterations, 20u);

  for (size_t i = 0; i < num_iterations; i++) {
    DisconnectAndRestartMetricsService();
    EXPECT_FALSE(service_binding_->has_clients()) << "Iteration " << i;
    task_environment_.FastForwardBy(LegacyMetricsClient::kMaxReconnectDelay);
    EXPECT_TRUE(service_binding_->has_clients()) << "Iteration " << i;
  }
}

TEST_F(LegacyMetricsClientTest, RecordCompletionResetsReconnectDelay) {
  StartClientAndExpectConnection();

  // First reconnect has initial delay.
  DisconnectAndRestartMetricsService();
  ExpectReconnectAfterDelay(LegacyMetricsClient::kInitialReconnectDelay);

  // Another reconnect without a successful Record() call increases the delay.
  DisconnectAndRestartMetricsService();
  ExpectReconnectAfterDelay(LegacyMetricsClient::kInitialReconnectDelay *
                            LegacyMetricsClient::kReconnectBackoffFactor);

  // Record and report an event, invoking a FIDL Record().
  base::RecordComputedAction("ArbitraryEvent");
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();

  // Reconnect after a successful Record() uses the initial delay again.
  DisconnectAndRestartMetricsService();
  ExpectReconnectAfterDelay(LegacyMetricsClient::kInitialReconnectDelay);
}

TEST_F(LegacyMetricsClientTest, ContinueRecordingUserActionsAfterDisconnect) {
  StartClientAndExpectConnection();

  base::RecordComputedAction("BeforeDisconnect");
  DisconnectAndRestartMetricsService();
  base::RecordComputedAction("DuringDisconnect");
  ExpectReconnectAfterDelay(LegacyMetricsClient::kInitialReconnectDelay);
  base::RecordComputedAction("AfterReconnect");

  // Fast forward to report metrics.
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_THAT(
      events,
      UnorderedElementsAreArray({
          Property(&fuchsia::legacymetrics::Event::user_action_event,
                   Property(&fuchsia::legacymetrics::UserActionEvent::name,
                            "BeforeDisconnect")),
          Property(&fuchsia::legacymetrics::Event::user_action_event,
                   Property(&fuchsia::legacymetrics::UserActionEvent::name,
                            "DuringDisconnect")),
          Property(&fuchsia::legacymetrics::Event::user_action_event,
                   Property(&fuchsia::legacymetrics::UserActionEvent::name,
                            "AfterReconnect")),
      }));
}

TEST_F(LegacyMetricsClientTest, Batching) {
  client_.Start(kReportInterval);

  // Log enough actions that the list will be split across multiple batches.
  // Batches are read out in reverse order, so even though it is being logged
  // first, it will be emitted in the final batch.
  base::RecordComputedAction("batch2");

  for (size_t i = 0; i < LegacyMetricsClient::kMaxBatchSize; ++i)
    base::RecordComputedAction("batch1");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  // First batch.
  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(LegacyMetricsClient::kMaxBatchSize, events.size());
  for (const auto& event : events)
    EXPECT_EQ(event.user_action_event().name(), "batch1");
  test_recorder_.SendAck();

  // Second batch (remainder).
  events = test_recorder_.WaitForEvents();
  EXPECT_EQ(1u, events.size());
  for (const auto& event : events)
    EXPECT_EQ(event.user_action_event().name(), "batch2");
  test_recorder_.SendAck();
}

TEST_F(LegacyMetricsClientTest, FlushWithPending) {
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);

  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  service_binding_->events().OnCloseSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  // The service should be unbound once all data is drained.
  EXPECT_TRUE(service_binding_->has_clients());
  auto events = test_recorder_.WaitForEvents();
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("foo", events[0].histogram().name());
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, FlushNoData) {
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service_binding_->has_clients());
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  service_binding_->events().OnCloseSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, FlushWithOutstandingAck) {
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  // Send "foo", but don't ack.
  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  // Allow the flush operation to call Record() without waiting for a prior ack.
  test_recorder_.set_expect_ack_dropped(true);

  // Buffer another event and trigger a flush.
  UMA_HISTOGRAM_COUNTS_1M("bar", 20);
  EXPECT_TRUE(service_binding_->has_clients());
  service_binding_->events().OnCloseSoon();

  // Simulate an asynchronous ack from the recorder, which be delivered around
  // the same time as the flush's Record() call. The ack should be gracefully
  // ignored by the client.
  test_recorder_.SendAck();

  base::RunLoop().RunUntilIdle();

  auto events = test_recorder_.WaitForEvents();
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ("foo", events[0].histogram().name());
  EXPECT_EQ("bar", events[1].histogram().name());
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, ExternalFlushSignal) {
  base::test::TestFuture<base::OnceClosure> flush_receiver;
  client_.SetNotifyFlushCallback(flush_receiver.GetCallback());
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);

  // Verify that reporting does not start until the flush completion callback is
  // run.
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  service_binding_->events().OnCloseSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  // Verify that invoking the completion callback unblocks reporting.
  EXPECT_TRUE(flush_receiver.IsReady());
  flush_receiver.Take().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, ExplicitFlush) {
  client_.Start(kReportInterval);

  base::RecordComputedAction("bar");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  bool called = false;
  client_.FlushAndDisconnect(
      base::BindLambdaForTesting([&called] { called = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  EXPECT_FALSE(called);

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("bar", events[0].user_action_event().name());

  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(LegacyMetricsClientTest, DoubleFlush) {
  client_.Start(kReportInterval);

  base::RecordComputedAction("bar");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  bool called = false;
  client_.FlushAndDisconnect(
      base::BindLambdaForTesting([&called] { called = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  EXPECT_FALSE(called);

  bool called2 = false;
  client_.FlushAndDisconnect(
      base::BindLambdaForTesting([&called2] { called2 = true; }));

  test_recorder_.WaitForEvents();
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();

  // Verify that both FlushAndDisconnect() callbacks were called.
  EXPECT_TRUE(called);
  EXPECT_TRUE(called2);
}

TEST_F(LegacyMetricsClientTest, ExplicitFlushMultipleBatches) {
  const size_t kSizeForMultipleBatches = LegacyMetricsClient::kMaxBatchSize * 2;
  client_.Start(kReportInterval);

  for (size_t i = 0; i < kSizeForMultipleBatches; ++i)
    base::RecordComputedAction("bar");

  client_.FlushAndDisconnect(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(kSizeForMultipleBatches, events.size());
  for (size_t i = 0; i < kSizeForMultipleBatches; ++i)
    EXPECT_EQ("bar", events[i].user_action_event().name());
}

TEST_F(LegacyMetricsClientTest, UseInjectedMetricsRecorder) {
  client_.DisableAutoConnect();
  SetMetricsRecorder();

  client_.Start(kReportInterval);

  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("bar", events[0].user_action_event().name());

  // Verify that /svc wasn't used.
  EXPECT_FALSE(service_binding_->has_clients());

  // Verify that LegacyMetricsClient doesn't try to reconnect after
  // MetricsRecorder has been disconnected.
  direct_binding_.Unbind();
  task_environment_.FastForwardBy(LegacyMetricsClient::kInitialReconnectDelay *
                                  2);
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, UseInjectedMetricsRecorderReconnect) {
  client_.DisableAutoConnect();
  SetMetricsRecorder();

  client_.Start(kReportInterval);

  bool flush_complete = false;
  client_.FlushAndDisconnect(
      base::BindLambdaForTesting([&flush_complete] { flush_complete = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(flush_complete);

  EXPECT_TRUE(test_recorder_.IsEmpty());

  // Set recorder again and verify that it receives metrics now.
  SetMetricsRecorder();

  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(1u, events.size());
}

TEST_F(LegacyMetricsClientTest, SetMetricsRecorderDuringRecord) {
  client_.DisableAutoConnect();
  SetMetricsRecorder();

  client_.Start(kReportInterval);

  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.DropAck();

  // Set recorder again and verify that it can receive metrics.
  SetMetricsRecorder();

  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(2u, events.size());
}

TEST_F(LegacyMetricsClientTest, SetMetricsRecorderDuringFlush) {
  client_.DisableAutoConnect();
  SetMetricsRecorder();

  client_.Start(kReportInterval);

  base::RecordComputedAction("bar");

  bool flush_complete = false;
  client_.FlushAndDisconnect(
      base::BindLambdaForTesting([&flush_complete] { flush_complete = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.DropAck();
  EXPECT_FALSE(flush_complete);

  // Set recorder again. It's expected to complete the Flush().
  SetMetricsRecorder();
  EXPECT_TRUE(flush_complete);

  // Verify that metrics are sent to the new MetricsRecorder instance.
  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(2u, events.size());
}

}  // namespace
}  // namespace fuchsia_legacymetrics
