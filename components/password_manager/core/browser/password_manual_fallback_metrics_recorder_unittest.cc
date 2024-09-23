// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
constexpr auto kLatencyDelta = base::Milliseconds(123u);
constexpr char kShowSuggestionLatency[] =
    "PasswordManager.ManualFallback.ShowSuggestions.Latency";
}  // namespace

class PasswordManualFallbackMetricsRecorderTest : public testing::Test {
 public:
  PasswordManualFallbackMetricsRecorderTest() = default;
  ~PasswordManualFallbackMetricsRecorderTest() override = default;

  void AdvanceClock(base::TimeDelta millis) {
    task_environment_.FastForwardBy(millis);
  }

  PasswordManualFallbackMetricsRecorder& metrics_recorder() {
    return metrics_recorder_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  PasswordManualFallbackMetricsRecorder metrics_recorder_;
};

TEST_F(PasswordManualFallbackMetricsRecorderTest, RecordDataFetchingLatency) {
  base::HistogramTester histogram_tester;

  metrics_recorder().DataFetchingStarted();
  AdvanceClock(kLatencyDelta);
  metrics_recorder().RecordDataFetchingLatency();

  histogram_tester.ExpectTotalCount(kShowSuggestionLatency, 1);
  histogram_tester.ExpectTimeBucketCount(kShowSuggestionLatency, kLatencyDelta,
                                         1);
}

}  // namespace password_manager
