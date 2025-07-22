// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestMetricName[] = "Test.";
const char kComposeboxSessionDurationTotal[] =
    "Test.Composebox.Session.Duration.Total";
const char kComposeboxSessionAbandonedDuration[] =
    "Test.Composebox.Session.Duration.Abandoned";
const char kComposeboxSessionDurationCompleted[] =
    "Test.Composebox.Session.Duration.Completed";
const char kComposeboxQuerySubmissionTime[] =
    "Test.Composebox.Query.Time.ToSubmission";
}  // namespace

class ComposeboxMetricsRecorderTest : public testing::Test {
 public:
  ComposeboxMetricsRecorderTest() = default;
  ~ComposeboxMetricsRecorderTest() override = default;

  void SetUp() override {
    metrics_recorder_ =
        std::make_unique<ComposeboxMetricsRecorder>(kTestMetricName);
  }

  ComposeboxMetricsRecorder& metrics() { return *metrics_recorder_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void DestructMetricsRecorder() { metrics_recorder_.reset(); }

 private:
  std::unique_ptr<ComposeboxMetricsRecorder> metrics_recorder_;
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ComposeboxMetricsRecorderTest, SessionAbandoned) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(60));
  metrics().NotifySessionStateChanged(SessionState::kSessionAbandoned);

  histogram_tester().ExpectTotalCount(kComposeboxSessionAbandonedDuration, 1);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationTotal, 1);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionAbandonedDuration,
                                            base::Seconds(60), 1);
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationTotal,
                                            base::Seconds(60), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, SessionCompleted) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(10));
  metrics().NotifySessionStateChanged(SessionState::kQuerySubmitted);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationCompleted, 1);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationTotal, 1);
  histogram_tester().ExpectTotalCount(kComposeboxQuerySubmissionTime, 1);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationCompleted,
                                            base::Seconds(10), 1);
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationTotal,
                                            base::Seconds(10), 1);
  // Check query submission time.
  histogram_tester().ExpectUniqueTimeSample(kComposeboxQuerySubmissionTime,
                                            base::Seconds(10), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, MultiQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  metrics().NotifySessionStateChanged(SessionState::kQuerySubmitted);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  // Mimic the session remaining open when the AIM page is opened in another
  // tab/window. In this case more queries can be submitted.
  task_environment().FastForwardBy(base::Seconds(60));
  metrics().NotifySessionStateChanged(SessionState::kQuerySubmitted);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationCompleted, 2);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationTotal, 2);
  histogram_tester().ExpectTotalCount(kComposeboxQuerySubmissionTime, 2);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationCompleted,
                                            base::Seconds(90), 2);
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationTotal,
                                            base::Seconds(90), 2);
  // Check query submission times.
  histogram_tester().ExpectTimeBucketCount(kComposeboxQuerySubmissionTime,
                                           base::Seconds(30), 1);
  histogram_tester().ExpectTimeBucketCount(kComposeboxQuerySubmissionTime,
                                           base::Seconds(90), 1);
}
