// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/free_memory_pressure_evaluator.h"

#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/memory/available_memory_monitor.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

class TestFreeMemoryPressureEvaluator : public FreeMemoryPressureEvaluator {
 public:
  using FreeMemoryPressureEvaluator::CalculateCurrentPressureLevel;

  explicit TestFreeMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : FreeMemoryPressureEvaluator(std::move(voter)) {
    StopObserving();
  }

  TestFreeMemoryPressureEvaluator(base::ByteSize moderate_threshold,
                                  base::ByteSize critical_threshold)
      : FreeMemoryPressureEvaluator(moderate_threshold,
                                    critical_threshold,
                                    nullptr) {
    StopObserving();
  }

  TestFreeMemoryPressureEvaluator(const TestFreeMemoryPressureEvaluator&) =
      delete;
  TestFreeMemoryPressureEvaluator& operator=(
      const TestFreeMemoryPressureEvaluator&) = delete;

  base::AvailableMemoryMonitor::MemorySample CreateSample(
      base::ByteSize free_phys) {
    base::AvailableMemoryMonitor::MemorySample sample;
    sample.timestamp = base::TimeTicks::Now();
    sample.free_physical_bytes = free_phys;
    return sample;
  }
};

class FreeMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestFreeMemoryPressureEvaluator* evaluator) {
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

TEST_F(FreeMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelDefault) {
  static constexpr base::ByteSize kModerate =
      FreeMemoryPressureEvaluator::kDefaultModerateThreshold;
  static constexpr base::ByteSize kCritical =
      FreeMemoryPressureEvaluator::kDefaultCriticalThreshold;

  TestFreeMemoryPressureEvaluator evaluator(nullptr);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  CalculateCurrentMemoryPressureLevelTest(&evaluator);
}

TEST_F(FreeMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static constexpr base::ByteSize kModerate = base::MiB(200);
  static constexpr base::ByteSize kCritical = base::MiB(100);

  TestFreeMemoryPressureEvaluator evaluator(kModerate, kCritical);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  CalculateCurrentMemoryPressureLevelTest(&evaluator);
}

TEST_F(FreeMemoryPressureEvaluatorTest, CreateWithMacFeatureParamsDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kMacFreeMemoryPressureEvaluator);

  auto evaluator =
      FreeMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(base::MiB(150), evaluator->moderate_threshold());
  EXPECT_EQ(base::MiB(50), evaluator->critical_threshold());
}

TEST_F(FreeMemoryPressureEvaluatorTest, CreateWithMacFeatureParamsCustom) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kMacFreeMemoryPressureEvaluator,
      {{"MacFreeModerateThresholdMB", "200"},
       {"MacFreeCriticalThresholdMB", "100"}});

  auto evaluator =
      FreeMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(base::MiB(200), evaluator->moderate_threshold());
  EXPECT_EQ(base::MiB(100), evaluator->critical_threshold());
}

TEST_F(FreeMemoryPressureEvaluatorTest, CreateWithMacFeatureParamsInvalid) {
  // Test inverted thresholds (Critical > Moderate) to verify defensive
  // fallback.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kMacFreeMemoryPressureEvaluator,
        {{"MacFreeModerateThresholdMB", "50"},
         {"MacFreeCriticalThresholdMB", "100"}});

    auto evaluator =
        FreeMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
    ASSERT_TRUE(evaluator);
    EXPECT_EQ(base::MiB(150), evaluator->moderate_threshold());
    EXPECT_EQ(base::MiB(50), evaluator->critical_threshold());
  }

  // Test non-positive critical threshold.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kMacFreeMemoryPressureEvaluator,
        {{"MacFreeModerateThresholdMB", "150"},
         {"MacFreeCriticalThresholdMB", "-100"}});

    auto evaluator =
        FreeMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
    ASSERT_TRUE(evaluator);
    EXPECT_EQ(base::MiB(150), evaluator->moderate_threshold());
    EXPECT_EQ(base::MiB(50), evaluator->critical_threshold());
  }

  // Test non-positive moderate threshold.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kMacFreeMemoryPressureEvaluator,
        {{"MacFreeModerateThresholdMB", "-100"},
         {"MacFreeCriticalThresholdMB", "50"}});

    auto evaluator =
        FreeMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
    ASSERT_TRUE(evaluator);
    EXPECT_EQ(base::MiB(150), evaluator->moderate_threshold());
    EXPECT_EQ(base::MiB(50), evaluator->critical_threshold());
  }
}

}  // namespace memory_pressure
