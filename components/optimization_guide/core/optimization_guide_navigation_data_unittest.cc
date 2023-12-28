// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_navigation_data.h"

#include <memory>

#include "base/base64.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyOf;
using testing::HasSubstr;
using testing::Not;

TEST(OptimizationGuideNavigationDataTest, RecordMetricsNoData) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/base::TimeTicks::Now());
  data.reset();

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigtaionDataTest,
     RecordMetricsRegisteredOptimizationTypes) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/base::TimeTicks::Now());
  data->set_registered_optimization_types(
      {optimization_guide::proto::NOSCRIPT,
       optimization_guide::proto::RESOURCE_LOADING});
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  // The bitmask should be 110 since NOSCRIPT=1 and RESOURCE_LOADING=2.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName,
      6);
}

TEST(OptimizationGuideNavigtaionDataTest,
     RecordMetricsRegisteredOptimizationTargets) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/base::TimeTicks::Now());
  data->set_registered_optimization_targets(
      {optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN,
       optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTargetsName));
  // The bitmask should be 11 since UNKNOWN=0 and PAINFUL_PAGE_LOAD=1.
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTargetsName, 3);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchAttemptStatusForNavigation) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/base::TimeTicks::Now());
  data->set_hints_fetch_attempt_status(
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchHost);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchAttemptStatusName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchAttemptStatusName,
      static_cast<int>(optimization_guide::RaceNavigationFetchAttemptStatus::
                           kRaceNavigationFetchHost));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigation) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::TimeTicks now = base::TimeTicks::Now();
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/now);
  data->set_hints_fetch_start(now);
  data->set_hints_fetch_end(now + base::Milliseconds(123));
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchRequestLatencyName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchRequestLatencyName,
      123);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigationNoStart) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::TimeTicks now = base::TimeTicks::Now();
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/now);
  data->set_hints_fetch_end(now);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigationNoEnd) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::TimeTicks now = base::TimeTicks::Now();
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/now);
  data->set_hints_fetch_start(now);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchRequestLatencyName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchRequestLatencyName,
      INT64_MAX);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigationEndLessThanStart) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::TimeTicks now = base::TimeTicks::Now();
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(
          /*navigation_id=*/3, /*navigation_start=*/now);
  data->set_hints_fetch_start(now);
  data->set_hints_fetch_end(now - base::Milliseconds(123));
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchRequestLatencyName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchRequestLatencyName,
      INT64_MAX);
}
