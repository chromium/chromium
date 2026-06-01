// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

class PrivateInsightsServiceTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PrivateInsightsServiceTest,
       TriggerUploadSkipsPostingTaskWhenAlreadyRunning) {
  base::HistogramTester histogram_tester;
  PrivateInsightsService service;

  // First call: should post the task.
  service.TriggerUpload();
  histogram_tester.ExpectUniqueSample(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 1);

  // Second call: while task is running, should be skipped.
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kSkippedAlreadyRunning, 1);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 2);

  // Wait for task execution to complete.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));

  // Third call: now that task completed, should post the task again.
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 2);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 3);
}

}  // namespace private_insights
