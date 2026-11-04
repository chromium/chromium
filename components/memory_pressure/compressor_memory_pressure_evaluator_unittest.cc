// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/compressor_memory_pressure_evaluator.h"

#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/memory/available_memory_monitor.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

class TestCompressorMemoryPressureEvaluator
    : public CompressorMemoryPressureEvaluator {
 public:
  using CompressorMemoryPressureEvaluator::CalculateCurrentPressureLevel;

  explicit TestCompressorMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : CompressorMemoryPressureEvaluator(std::move(voter)) {
    StopObserving();
  }

  explicit TestCompressorMemoryPressureEvaluator(int critical_threshold_percent)
      : CompressorMemoryPressureEvaluator(critical_threshold_percent, nullptr) {
    StopObserving();
  }

  TestCompressorMemoryPressureEvaluator(
      const TestCompressorMemoryPressureEvaluator&) = delete;
  TestCompressorMemoryPressureEvaluator& operator=(
      const TestCompressorMemoryPressureEvaluator&) = delete;

  base::AvailableMemoryMonitor::MemorySample CreateSample(
      base::ByteSize compressor_bytes,
      base::ByteSize total_physical_bytes = base::GiB(16),
      base::ByteSize wired_bytes = base::GiB(2)) {
    base::AvailableMemoryMonitor::MemorySample sample;
    sample.timestamp = base::TimeTicks::Now();
    sample.total_physical_bytes = total_physical_bytes;
    sample.compressor_bytes = compressor_bytes;
    sample.wired_bytes = wired_bytes;
    return sample;
  }
};

}  // namespace

class CompressorMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestCompressorMemoryPressureEvaluator* evaluator) {
    int critical_percent = evaluator->critical_threshold_percent();
    base::ByteSize total = base::GiB(16);
    base::ByteSize wired = base::GiB(2);
    base::ByteSize unwired = base::ByteSize::FromByteSizeDelta(total - wired);

    // 0 bytes compressor (0%) -> None.
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(base::ByteSize(0), total, wired)));

    // Below critical threshold (e.g. 43% of unwired memory for 45% default) ->
    // None.
    base::ByteSize below_critical = unwired * (critical_percent - 2) / 100;
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(below_critical, total, wired)));

    // Above critical threshold (e.g. 48% of unwired memory for 45% default) ->
    // Critical.
    base::ByteSize above_critical = unwired * (critical_percent + 3) / 100;
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(above_critical, total, wired)));

    // High saturation (75% of unwired memory) -> Critical.
    base::ByteSize high_saturation = unwired * 75 / 100;
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(high_saturation, total, wired)));
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(CompressorMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelDefault) {
  static constexpr int kCriticalPercent =
      CompressorMemoryPressureEvaluator::kDefaultCriticalThresholdPercent;

  TestCompressorMemoryPressureEvaluator evaluator(kCriticalPercent);

  EXPECT_EQ(kCriticalPercent, evaluator.critical_threshold_percent());

  CalculateCurrentMemoryPressureLevelTest(&evaluator);
}

TEST_F(CompressorMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static constexpr int kCriticalPercent = 50;

  TestCompressorMemoryPressureEvaluator evaluator(kCriticalPercent);

  EXPECT_EQ(kCriticalPercent, evaluator.critical_threshold_percent());

  CalculateCurrentMemoryPressureLevelTest(&evaluator);
}

TEST_F(CompressorMemoryPressureEvaluatorTest, WiredMemoryDiscounting) {
  TestCompressorMemoryPressureEvaluator evaluator(50);

  base::ByteSize total = base::GiB(16);
  base::ByteSize wired = base::GiB(2);

  // 16 GiB Total - 2 GiB Wired = 14 GiB Unwired.
  // 50% of 14 GiB is 7 GiB.
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.CalculateCurrentPressureLevel(
                evaluator.CreateSample(base::MiB(5500), total, wired)));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.CalculateCurrentPressureLevel(
                evaluator.CreateSample(base::MiB(7500), total, wired)));

  // If wired memory increases to 6 GiB, unwired memory drops to 10 GiB.
  // 50% of 10 GiB is 5 GiB.
  base::ByteSize higher_wired = base::GiB(6);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.CalculateCurrentPressureLevel(
                evaluator.CreateSample(base::MiB(5500), total, higher_wired)));

  // Edge case: wired memory >= total physical memory -> None (non-positive
  // unwired RAM).
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.CalculateCurrentPressureLevel(
                evaluator.CreateSample(base::GiB(8), total, total)));
}

TEST_F(CompressorMemoryPressureEvaluatorTest,
       CreateWithMacFeatureParamsDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kMacCompressorMemoryPressureEvaluator);

  auto evaluator =
      CompressorMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(45, evaluator->critical_threshold_percent());
}

TEST_F(CompressorMemoryPressureEvaluatorTest,
       CreateWithMacFeatureParamsCustom) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kMacCompressorMemoryPressureEvaluator,
      {{"MacCompressorCriticalThresholdPercent", "55"}});

  auto evaluator =
      CompressorMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(55, evaluator->critical_threshold_percent());
}

TEST_F(CompressorMemoryPressureEvaluatorTest,
       CreateWithMacFeatureParamsInvalid) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Test invalid threshold (> 100) to verify defensive fallback.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kMacCompressorMemoryPressureEvaluator,
      {{"MacCompressorCriticalThresholdPercent", "150"}});

  auto evaluator =
      CompressorMemoryPressureEvaluator::CreateWithMacFeatureParams(nullptr);
  ASSERT_TRUE(evaluator);
  EXPECT_EQ(45, evaluator->critical_threshold_percent());
}

}  // namespace memory_pressure
