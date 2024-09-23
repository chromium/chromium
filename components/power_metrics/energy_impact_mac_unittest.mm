// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/power_metrics/energy_impact_mac.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "components/power_metrics/resource_coalition_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_metrics {

namespace {

constexpr mach_timebase_info_data_t kIntelTimebase = {1, 1};
constexpr mach_timebase_info_data_t kM1Timebase = {125, 3};

base::FilePath GetTestDataPath() {
  base::FilePath test_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_path));
  test_path = test_path.Append(FILE_PATH_LITERAL("components"));
  test_path = test_path.Append(FILE_PATH_LITERAL("power_metrics"));
  test_path = test_path.Append(FILE_PATH_LITERAL("test"));
  test_path = test_path.Append(FILE_PATH_LITERAL("data"));
  return test_path;
}

coalition_resource_usage MakeResourceUsageWithQOS(int qos_level,
                                                  base::TimeDelta cpu_time) {
  coalition_resource_usage result{};
  result.cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
  result.cpu_time_eqos[qos_level] = cpu_time.InNanoseconds();
  return result;
}

// Scales a time given in ns to mach_time in |timebase|.
uint64_t NsScaleToTimebase(const mach_timebase_info_data_t& timebase,
                           int64_t time_ns) {
  return time_ns * timebase.denom / timebase.numer;
}

}  // namespace

TEST(EnergyImpactTest, ReadCoefficientsFromPath) {
  base::FilePath test_path = GetTestDataPath();

  // Validate that attempting to read from a non-existent file fails.
  auto coefficients = internal::ReadCoefficientsFromPath(
      test_path.Append(FILE_PATH_LITERAL("does-not-exist.plist")));
  EXPECT_FALSE(coefficients.has_value());

  // Validate that a well-formed file returns the expected coefficients.
  coefficients = internal::ReadCoefficientsFromPath(
      test_path.Append(FILE_PATH_LITERAL("test.plist")));
  ASSERT_TRUE(coefficients.has_value());

  const EnergyImpactCoefficients& value = coefficients.value();
  EXPECT_FLOAT_EQ(value.kcpu_time, 1.23);
  EXPECT_FLOAT_EQ(value.kdiskio_bytesread, 7.89);
  EXPECT_FLOAT_EQ(value.kdiskio_byteswritten, 1.2345);
  EXPECT_FLOAT_EQ(value.kgpu_time, 6.789);
  EXPECT_FLOAT_EQ(value.knetwork_recv_bytes, 12.3);
  EXPECT_FLOAT_EQ(value.knetwork_recv_packets, 45.6);
  EXPECT_FLOAT_EQ(value.knetwork_sent_bytes, 67.8);
  EXPECT_FLOAT_EQ(value.knetwork_sent_packets, 89);
  EXPECT_FLOAT_EQ(value.kqos_background, 8.9);
  EXPECT_FLOAT_EQ(value.kqos_default, 6.78);
  EXPECT_FLOAT_EQ(value.kqos_legacy, 5.678);
  EXPECT_FLOAT_EQ(value.kqos_user_initiated, 9.012);
  EXPECT_FLOAT_EQ(value.kqos_user_interactive, 3.456);
  EXPECT_FLOAT_EQ(value.kqos_utility, 1.234);
  EXPECT_FLOAT_EQ(value.kcpu_wakeups, 3.45);
}

TEST(EnergyImpactTest, ReadCoefficientsForBoardIdOrDefault_Exists) {
  // This board-id should exist.
  auto coefficients = internal::ReadCoefficientsForBoardIdOrDefault(
      GetTestDataPath(), "Mac-7BA5B2DFE22DDD8C");
  ASSERT_TRUE(coefficients.has_value());

  // Validate that the default coefficients haven't been loaded.
  EXPECT_FLOAT_EQ(3.4, coefficients.value().kgpu_time);
  EXPECT_FLOAT_EQ(0.39, coefficients.value().kqos_background);
}

TEST(EnergyImpactTest, ReadCoefficientsForBoardIdOrDefault_Default) {
  // This board-id should not exist.
  auto coefficients = internal::ReadCoefficientsForBoardIdOrDefault(
      GetTestDataPath(), "Mac-031B6874CF7F642A");
  ASSERT_TRUE(coefficients.has_value());

  // Validate that the default coefficients were loaded.
  EXPECT_FLOAT_EQ(0, coefficients.value().kgpu_time);
  EXPECT_FLOAT_EQ(0.8, coefficients.value().kqos_background);
}

TEST(EnergyImpactTest,
     ReadCoefficientsForBoardIdOrDefault_NonExistentDirectory) {
  // This directory shouldn't exist, so nothing should be loaded.
  EXPECT_FALSE(
      internal::ReadCoefficientsForBoardIdOrDefault(
          GetTestDataPath().Append("nonexistent"), "Mac-7BA5B2DFE22DDD8C")
          .has_value());
}

TEST(EnergyImpactTest, GetBoardIdForThisMachine) {
  // This can't really be tested except that the contract holds one way
  // or the other.
  auto board_id = internal::GetBoardIdForThisMachine();
  if (board_id.has_value()) {
    EXPECT_FALSE(board_id.value().empty());
  }
}

// Verify the Energy Impact score when there is a single source of energy
// consumption (only one member set in `coalition_resource_usage`).
TEST(EnergyImpactTest, ComputeEnergyImpactForResourceUsage_Individual) {
  EXPECT_EQ(0.0, ComputeEnergyImpactForResourceUsage(coalition_resource_usage(),
                                                     EnergyImpactCoefficients{},
                                                     kIntelTimebase));

  // Test the coefficients and sample factors individually.
  EXPECT_DOUBLE_EQ(
      2.66, ComputeEnergyImpactForResourceUsage(
                coalition_resource_usage{.platform_idle_wakeups = 133},
                EnergyImpactCoefficients{
                    .kcpu_wakeups = base::Microseconds(200).InSecondsF()},
                kIntelTimebase));

  // Test 100 ms of CPU, which should come out to 8% of a CPU second with a
  // background QOS discount of rate of 0.8.
  EXPECT_DOUBLE_EQ(8.0, ComputeEnergyImpactForResourceUsage(
                            MakeResourceUsageWithQOS(THREAD_QOS_BACKGROUND,
                                                     base::Milliseconds(100)),
                            EnergyImpactCoefficients{.kqos_background = 0.8},
                            kIntelTimebase));
  EXPECT_DOUBLE_EQ(
      5.0,
      ComputeEnergyImpactForResourceUsage(
          MakeResourceUsageWithQOS(THREAD_QOS_DEFAULT, base::Milliseconds(50)),
          EnergyImpactCoefficients{.kqos_default = 1.0}, kIntelTimebase));
  EXPECT_DOUBLE_EQ(
      10.0,
      ComputeEnergyImpactForResourceUsage(
          MakeResourceUsageWithQOS(THREAD_QOS_UTILITY, base::Milliseconds(100)),
          EnergyImpactCoefficients{.kqos_utility = 1.0}, kIntelTimebase));
  EXPECT_DOUBLE_EQ(
      1.0,
      ComputeEnergyImpactForResourceUsage(
          MakeResourceUsageWithQOS(THREAD_QOS_LEGACY, base::Milliseconds(10)),
          EnergyImpactCoefficients{.kqos_legacy = 1.0}, kIntelTimebase));
  EXPECT_DOUBLE_EQ(1.0,
                   ComputeEnergyImpactForResourceUsage(
                       MakeResourceUsageWithQOS(THREAD_QOS_USER_INITIATED,
                                                base::Milliseconds(10)),
                       EnergyImpactCoefficients{.kqos_user_initiated = 1.0},
                       kIntelTimebase));
  EXPECT_DOUBLE_EQ(1.0,
                   ComputeEnergyImpactForResourceUsage(
                       MakeResourceUsageWithQOS(THREAD_QOS_USER_INTERACTIVE,
                                                base::Milliseconds(10)),
                       EnergyImpactCoefficients{.kqos_user_interactive = 1.0},
                       kIntelTimebase));
  EXPECT_DOUBLE_EQ(
      1.0, ComputeEnergyImpactForResourceUsage(
               coalition_resource_usage{
                   .gpu_time = base::Milliseconds(4).InNanoseconds()},
               EnergyImpactCoefficients{.kgpu_time = 2.5}, kIntelTimebase));
}

// Verify the Energy Impact score when there are multiple sources of energy
// consumption (multiple members set in `coalition_resource_usage`).
TEST(EnergyImpactTest, ComputeEnergyImpactForResourceUsage_Combined) {
  EnergyImpactCoefficients coefficients{
      .kcpu_wakeups = base::Microseconds(200).InSecondsF(),
      .kqos_default = 1.0,
      .kqos_background = 0.8,
      .kqos_utility = 1.0,
      .kqos_legacy = 1.0,
      .kqos_user_initiated = 1.0,
      .kqos_user_interactive = 1.0,
      .kgpu_time = 2.5,
  };
  coalition_resource_usage sample{
      .platform_idle_wakeups = 133,
      .gpu_time =
          NsScaleToTimebase(kM1Timebase, base::Milliseconds(4).InNanoseconds()),
      .cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES,
  };
  sample.cpu_time_eqos[THREAD_QOS_BACKGROUND] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(100).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_DEFAULT] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(50).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_UTILITY] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(100).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_LEGACY] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(10).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_USER_INITIATED] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(10).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(10).InNanoseconds());

  EXPECT_DOUBLE_EQ(29.66, ComputeEnergyImpactForResourceUsage(
                              sample, coefficients, kM1Timebase));
}

// Verify the Energy Impact score when fields of `coalition_resource_usage` that
// don't contribute to the score are set.
TEST(EnergyImpactTest, ComputeEnergyImpactForResourceUsage_Unused) {
  EnergyImpactCoefficients coefficients{
      .kdiskio_bytesread = 1000,
      .kdiskio_byteswritten = 1000,
      .knetwork_recv_bytes = 1000,
      .knetwork_recv_packets = 1000,
      .knetwork_sent_bytes = 1000,
      .knetwork_sent_packets = 1000,
  };
  coalition_resource_usage sample{
      .tasks_started = 1000,
      .tasks_exited = 1000,
      .time_nonempty = 1000,
      .cpu_time = 1000,
      .interrupt_wakeups = 1000,
      .bytesread = 1000,
      .byteswritten = 1000,
      .cpu_time_billed_to_me = 1000,
      .cpu_time_billed_to_others = 1000,
      .energy = 1000,
      .logical_immediate_writes = 1000,
      .logical_deferred_writes = 1000,
      .logical_invalidated_writes = 1000,
      .logical_metadata_writes = 1000,
      .logical_immediate_writes_to_external = 1000,
      .logical_deferred_writes_to_external = 1000,
      .logical_invalidated_writes_to_external = 1000,
      .logical_metadata_writes_to_external = 1000,
      .energy_billed_to_me = 1000,
      .energy_billed_to_others = 1000,
      .cpu_ptime = 1000,
      .cpu_instructions = 1000,
      .cpu_cycles = 1000,
      .fs_metadata_writes = 1000,
      .pm_writes = 1000,
  };

  EXPECT_EQ(0, ComputeEnergyImpactForResourceUsage(sample, coefficients,
                                                   kM1Timebase));
}

}  // namespace power_metrics
