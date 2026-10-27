// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/threshold_memory_pressure_evaluator.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/memory/available_memory_monitor.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr char kCommitLimitMBHistogramName[] = "Memory.CommitLimitMB";
constexpr char kCommitAvailableMBHistogramName[] = "Memory.CommitAvailableMB";
constexpr char kCommitPercentageUsedHistogramName[] =
    "Memory.CommitPercentageUsed";
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class TestThresholdMemoryPressureEvaluator
    : public ThresholdMemoryPressureEvaluator {
 public:
  using ThresholdMemoryPressureEvaluator::CalculateCurrentPressureLevel;
#if BUILDFLAG(IS_WIN)
  using ThresholdMemoryPressureEvaluator::RecordCommitHistograms;
#endif

  TestThresholdMemoryPressureEvaluator(base::ByteSize moderate_threshold,
                                       base::ByteSize critical_threshold)
      : ThresholdMemoryPressureEvaluator(moderate_threshold,
                                         critical_threshold,
                                         nullptr) {
    StopObserving();
  }

#if BUILDFLAG(IS_WIN)
  TestThresholdMemoryPressureEvaluator()
      : TestThresholdMemoryPressureEvaluator(kDefaultWinModerateThreshold,
                                             kDefaultWinCriticalThreshold) {}
#endif

  TestThresholdMemoryPressureEvaluator(
      const TestThresholdMemoryPressureEvaluator&) = delete;
  TestThresholdMemoryPressureEvaluator& operator=(
      const TestThresholdMemoryPressureEvaluator&) = delete;

  base::AvailableMemoryMonitor::MemorySample CreateSample(
      base::ByteSize phys_left
#if BUILDFLAG(IS_WIN)
      ,
      base::ByteSize commit_limit = base::GiB(4),
      base::ByteSize commit_available = base::GiB(2)
#endif
  ) {
    base::AvailableMemoryMonitor::MemorySample sample;
    sample.timestamp = base::TimeTicks::Now();
    sample.available_physical_bytes = phys_left;
#if BUILDFLAG(IS_WIN)
    sample.available_commit_bytes = commit_available;
    sample.total_commit_bytes = commit_limit;
#endif
    return sample;
  }
};

class ThresholdMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestThresholdMemoryPressureEvaluator* evaluator) {
    base::ByteSize moderate = evaluator->moderate_threshold();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(moderate + base::MiB(1))));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(moderate)));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel(evaluator->CreateSample(
                  (moderate - base::MiB(1)).AsByteSize())));

    base::ByteSize critical = evaluator->critical_threshold();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(critical + base::MiB(1))));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(critical)));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel(evaluator->CreateSample(
                  (critical - base::MiB(1)).AsByteSize())));
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(ThresholdMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevel) {
  static constexpr base::ByteSize kModerate = base::MiB(256);
  static constexpr base::ByteSize kCritical = base::MiB(128);

  TestThresholdMemoryPressureEvaluator evaluator(kModerate, kCritical);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

#if BUILDFLAG(IS_WIN)
TEST_F(ThresholdMemoryPressureEvaluatorTest, CreateWithWinDefaults) {
  auto evaluator =
      ThresholdMemoryPressureEvaluator::CreateWithWinDefaults(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(ThresholdMemoryPressureEvaluator::kDefaultWinModerateThreshold,
            evaluator->moderate_threshold());
  EXPECT_EQ(ThresholdMemoryPressureEvaluator::kDefaultWinCriticalThreshold,
            evaluator->critical_threshold());
}

TEST_F(ThresholdMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelWinDefaults) {
  TestThresholdMemoryPressureEvaluator evaluator;

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

TEST_F(ThresholdMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCommitEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCommitAvailableMemoryPressureThresholds,
      {{"CommitAvailableModerateThresholdMB", "500"},
       {"CommitAvailableCriticalThresholdMB", "200"}});

  TestThresholdMemoryPressureEvaluator evaluator;

  // Ample physical memory (2000 MiB), but moderate commit available (400 MiB).
  auto sample_mod = evaluator.CreateSample(
      /*phys_left=*/base::MiB(2000),
      /*commit_limit=*/base::GiB(4),
      /*commit_available=*/base::MiB(400));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator.CalculateCurrentPressureLevel(sample_mod));

  // Ample physical memory (2000 MiB), but critical commit available (150 MiB).
  auto sample_crit = evaluator.CreateSample(
      /*phys_left=*/base::MiB(2000),
      /*commit_limit=*/base::GiB(4),
      /*commit_available=*/base::MiB(150));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.CalculateCurrentPressureLevel(sample_crit));

  // Ample physical memory (2000 MiB), and ample commit available (600 MiB).
  auto sample_none = evaluator.CreateSample(
      /*phys_left=*/base::MiB(2000),
      /*commit_limit=*/base::GiB(4),
      /*commit_available=*/base::MiB(600));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.CalculateCurrentPressureLevel(sample_none));
}

TEST_F(ThresholdMemoryPressureEvaluatorTest, RecordCommitHistogramsBasic) {
  base::HistogramTester histogram_tester;
  TestThresholdMemoryPressureEvaluator evaluator;

  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/base::GiB(4),
                                       /*commit_available=*/base::GiB(2));

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 4096, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 2048, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 50,
                                      1);
}

TEST_F(ThresholdMemoryPressureEvaluatorTest,
       RecordCommitHistogramsDivisionByZero) {
  base::HistogramTester histogram_tester;
  TestThresholdMemoryPressureEvaluator evaluator;

  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/base::ByteSize(0),
                                       /*commit_available=*/base::ByteSize(0));

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

TEST_F(ThresholdMemoryPressureEvaluatorTest, RecordCommitHistogramsOverflow) {
  base::HistogramTester histogram_tester;
  TestThresholdMemoryPressureEvaluator evaluator;

  constexpr base::ByteSize kLargerThanMaxInt =
      base::MiB(static_cast<uint64_t>(std::numeric_limits<int>::max()) + 1U);
  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/kLargerThanMaxInt,
                                       /*commit_available=*/kLargerThanMaxInt);

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 10000000, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 10000000,
                                      1);
}

TEST_F(ThresholdMemoryPressureEvaluatorTest, PotentialUnderflow) {
  base::HistogramTester histogram_tester;
  TestThresholdMemoryPressureEvaluator evaluator;

  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/base::MiB(50),
                                       /*commit_available=*/base::MiB(100));

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 50, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 100, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
TEST_F(ThresholdMemoryPressureEvaluatorTest,
       CreateWithMacFeatureParamsDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kMacAvailableMemoryPressureEvaluator);

  auto evaluator =
      ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(base::MiB(1200), evaluator->moderate_threshold());
  EXPECT_EQ(base::MiB(800), evaluator->critical_threshold());
}

TEST_F(ThresholdMemoryPressureEvaluatorTest, CreateWithMacFeatureParamsCustom) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kMacAvailableMemoryPressureEvaluator,
      {{"MacModerateThresholdMB", "1500"}, {"MacCriticalThresholdMB", "1000"}});

  auto evaluator =
      ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(base::MiB(1500), evaluator->moderate_threshold());
  EXPECT_EQ(base::MiB(1000), evaluator->critical_threshold());
}

TEST_F(ThresholdMemoryPressureEvaluatorTest,
       CreateWithMacFeatureParamsInvalid) {
  // Test inverted thresholds (moderate < critical) to verify defensive
  // fallback.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kMacAvailableMemoryPressureEvaluator,
        {{"MacModerateThresholdMB", "400"}, {"MacCriticalThresholdMB", "800"}});

    auto evaluator =
        ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
    ASSERT_TRUE(evaluator);
    EXPECT_EQ(base::MiB(1200), evaluator->moderate_threshold());
    EXPECT_EQ(base::MiB(800), evaluator->critical_threshold());
  }

  // Test non-positive critical threshold.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kMacAvailableMemoryPressureEvaluator,
        {{"MacModerateThresholdMB", "1200"},
         {"MacCriticalThresholdMB", "-100"}});

    auto evaluator =
        ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
    ASSERT_TRUE(evaluator);
    EXPECT_EQ(base::MiB(1200), evaluator->moderate_threshold());
    EXPECT_EQ(base::MiB(800), evaluator->critical_threshold());
  }

  // Test non-positive moderate threshold.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kMacAvailableMemoryPressureEvaluator,
        {{"MacModerateThresholdMB", "-100"},
         {"MacCriticalThresholdMB", "800"}});

    auto evaluator =
        ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
    ASSERT_TRUE(evaluator);
    EXPECT_EQ(base::MiB(1200), evaluator->moderate_threshold());
    EXPECT_EQ(base::MiB(800), evaluator->critical_threshold());
  }
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace memory_pressure
