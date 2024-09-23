// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/power_metrics/resource_coalition_mac.h"

#include <optional>

#include "base/rand_util.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_metrics {

namespace {

constexpr mach_timebase_info_data_t kIntelTimebase = {1, 1};
constexpr mach_timebase_info_data_t kM1Timebase = {125, 3};

constexpr EnergyImpactCoefficients kEnergyImpactCoefficients{
    .kcpu_wakeups = base::Microseconds(200).InSecondsF(),
    .kqos_default = 1.0,
    .kqos_background = 0.8,
    .kqos_utility = 1.0,
    .kqos_legacy = 1.0,
    .kqos_user_initiated = 1.0,
    .kqos_user_interactive = 1.0,
    .kgpu_time = 2.5,
};

coalition_resource_usage GetTestCoalitionResourceUsage(uint32_t increment) {
  coalition_resource_usage ret{
      .tasks_started = 1 + increment,
      .tasks_exited = 2 + increment,
      .time_nonempty = 3 + increment,
      .cpu_time = 4 + increment,
      .interrupt_wakeups = 5 + increment,
      .platform_idle_wakeups = 6 + increment,
      .bytesread = 7 + increment,
      .byteswritten = 8 + increment,
      .gpu_time = 9 + increment,
      .cpu_time_billed_to_me = 10 + increment,
      .cpu_time_billed_to_others = 11 + increment,
      .energy = 12 + increment,
      .logical_immediate_writes = 13 + increment,
      .logical_deferred_writes = 14 + increment,
      .logical_invalidated_writes = 15 + increment,
      .logical_metadata_writes = 16 + increment,
      .logical_immediate_writes_to_external = 17 + increment,
      .logical_deferred_writes_to_external = 18 + increment,
      .logical_invalidated_writes_to_external = 19 + increment,
      .logical_metadata_writes_to_external = 20 + increment,
      .energy_billed_to_me = 21 + increment,
      .energy_billed_to_others = 22 + increment,
      .cpu_ptime = 23 + increment,
      .cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES,
      .cpu_instructions = 31 + increment,
      .cpu_cycles = 32 + increment,
      .fs_metadata_writes = 33 + increment,
      .pm_writes = 34 + increment,
  };

  ret.cpu_time_eqos[THREAD_QOS_DEFAULT] = 24 + increment;
  ret.cpu_time_eqos[THREAD_QOS_MAINTENANCE] = 25 + increment;
  ret.cpu_time_eqos[THREAD_QOS_BACKGROUND] = 26 + increment;
  ret.cpu_time_eqos[THREAD_QOS_UTILITY] = 27 + increment;
  ret.cpu_time_eqos[THREAD_QOS_LEGACY] = 28 + increment;
  ret.cpu_time_eqos[THREAD_QOS_USER_INITIATED] = 29 + increment;
  ret.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE] = 30 + increment;

  return ret;
}

void BurnCPU() {
  base::TimeTicks begin = base::TimeTicks::Now();
  constexpr base::TimeDelta busy_time = base::Seconds(1);
  [[maybe_unused]] volatile double number = 1;
  while (base::TimeTicks::Now() < (begin + busy_time)) {
    for (int i = 0; i < 10000; ++i)
      number = number * base::RandDouble();
  }
}

}  // namespace

// TODO(crbug.com/328102500): Test failing on Mac builders, hence disabled.
TEST(ResourceCoalitionMacTest, DISABLED_Busy) {
  std::optional<uint64_t> coalition_id =
      GetProcessCoalitionId(base::GetCurrentProcId());
  ASSERT_TRUE(coalition_id.has_value());

  std::unique_ptr<coalition_resource_usage> begin =
      GetCoalitionResourceUsage(coalition_id.value());
  BurnCPU();
  std::unique_ptr<coalition_resource_usage> end =
      GetCoalitionResourceUsage(coalition_id.value());

  ASSERT_TRUE(begin);
  ASSERT_TRUE(end);

  EXPECT_GT(end->cpu_instructions, begin->cpu_instructions);
  EXPECT_GT(end->cpu_cycles, begin->cpu_cycles);
  EXPECT_GT(end->cpu_time, begin->cpu_time);
}

TEST(ResourceCoalitionMacTest, Difference) {
  coalition_resource_usage left =
      GetTestCoalitionResourceUsage(/* increment= */ 1);
  coalition_resource_usage right =
      GetTestCoalitionResourceUsage(/* increment= */ 0);
  coalition_resource_usage diff =
      GetCoalitionResourceUsageDifference(left, right);

  EXPECT_EQ(diff.tasks_started, 1U);
  EXPECT_EQ(diff.tasks_exited, 1U);
  EXPECT_EQ(diff.time_nonempty, 1U);
  EXPECT_EQ(diff.cpu_time, 1U);
  EXPECT_EQ(diff.interrupt_wakeups, 1U);
  EXPECT_EQ(diff.platform_idle_wakeups, 1U);
  EXPECT_EQ(diff.bytesread, 1U);
  EXPECT_EQ(diff.byteswritten, 1U);
  EXPECT_EQ(diff.gpu_time, 1U);
  EXPECT_EQ(diff.cpu_time_billed_to_me, 1U);
  EXPECT_EQ(diff.cpu_time_billed_to_others, 1U);
  EXPECT_EQ(diff.energy, 1U);
  EXPECT_EQ(diff.logical_immediate_writes, 1U);
  EXPECT_EQ(diff.logical_deferred_writes, 1U);
  EXPECT_EQ(diff.logical_invalidated_writes, 1U);
  EXPECT_EQ(diff.logical_metadata_writes, 1U);
  EXPECT_EQ(diff.logical_immediate_writes_to_external, 1U);
  EXPECT_EQ(diff.logical_deferred_writes_to_external, 1U);
  EXPECT_EQ(diff.logical_invalidated_writes_to_external, 1U);
  EXPECT_EQ(diff.logical_metadata_writes_to_external, 1U);
  EXPECT_EQ(diff.energy_billed_to_me, 1U);
  EXPECT_EQ(diff.energy_billed_to_others, 1U);
  EXPECT_EQ(diff.cpu_ptime, 1U);
  EXPECT_EQ(diff.cpu_time_eqos_len,
            static_cast<uint64_t>(COALITION_NUM_THREAD_QOS_TYPES));

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i)
    EXPECT_EQ(diff.cpu_time_eqos[i], 1U);

  EXPECT_EQ(diff.cpu_instructions, 1U);
  EXPECT_EQ(diff.cpu_cycles, 1U);
  EXPECT_EQ(diff.fs_metadata_writes, 1U);
  EXPECT_EQ(diff.pm_writes, 1U);
}

namespace {

constexpr base::TimeDelta kIntervalDuration = base::Seconds(2.5);

constexpr double kExpectedCPUUsagePerSecondPercent = 0.7;
constexpr double kExpectedGPUUsagePerSecondPercent = 0.3;
// Note: The following counters must have an integral value once multiplied by
// the interval length in seconds (2.5).
constexpr double kExpectedInterruptWakeUpPerSecond = 0.4;
constexpr double kExpectedPlatformIdleWakeUpPerSecond = 10;
constexpr double kExpectedBytesReadPerSecond = 0.8;
constexpr double kExpectedBytesWrittenPerSecond = 1.6;
constexpr double kExpectedPowerNW = 10000.0;
// This number will be multiplied by the int value associated with a QoS level
// to compute the expected time spent in this QoS level. E.g.
// |QoSLevels::kUtility == 3| so the time spent in the utility QoS state will
// be set to 3 * 0.1 = 30%.
constexpr double kExpectedQoSTimeBucketIdMultiplier = 0.1;

// Scales a time given in ns to mach_time in |timebase|.
uint64_t NsScaleToTimebase(const mach_timebase_info_data_t& timebase,
                           int64_t time_ns) {
  return time_ns * timebase.denom / timebase.numer;
}

// Returns test data with all time quantities scaled to the given time base.
std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsageRateTestData(
    const mach_timebase_info_data_t& timebase) {
  std::unique_ptr<coalition_resource_usage> test_data =
      std::make_unique<coalition_resource_usage>();

  // Scales a time given in ns to mach_time in |timebase|.
  auto scale_to_timebase = [&timebase](double time_ns) -> int64_t {
    return NsScaleToTimebase(timebase, time_ns);
  };

  test_data->cpu_time = scale_to_timebase(kExpectedCPUUsagePerSecondPercent *
                                          kIntervalDuration.InNanoseconds());
  test_data->interrupt_wakeups =
      kExpectedInterruptWakeUpPerSecond * kIntervalDuration.InSecondsF();
  test_data->platform_idle_wakeups =
      kExpectedPlatformIdleWakeUpPerSecond * kIntervalDuration.InSecondsF();
  test_data->bytesread =
      kExpectedBytesReadPerSecond * kIntervalDuration.InSecondsF();
  test_data->byteswritten =
      kExpectedBytesWrittenPerSecond * kIntervalDuration.InSecondsF();
  test_data->gpu_time = scale_to_timebase(kExpectedGPUUsagePerSecondPercent *
                                          kIntervalDuration.InNanoseconds());
  test_data->energy = kExpectedPowerNW * kIntervalDuration.InSecondsF();
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    test_data->cpu_time_eqos[i] =
        scale_to_timebase(i * kExpectedQoSTimeBucketIdMultiplier *
                          kIntervalDuration.InNanoseconds());
  }
  test_data->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;

  return test_data;
}

}  // namespace

TEST(ResourceCoalitionMacTest, GetDataRate_NoEnergyImpact_Intel) {
  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageRateTestData(kIntelTimebase);

  auto rate = GetCoalitionResourceUsageRate(
      *t0_data, *t1_data, kIntervalDuration, kIntelTimebase, std::nullopt);
  ASSERT_TRUE(rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent, rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond, rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent, rate->gpu_time_per_second);
  EXPECT_FALSE(rate->energy_impact_per_second.has_value());
  EXPECT_EQ(kExpectedPowerNW, rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     rate->qos_time_per_second[i]);
  }
}

TEST(ResourceCoalitionMacTest, GetDataRate_NoEnergyImpact_M1) {
  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageRateTestData(kM1Timebase);

  auto rate = GetCoalitionResourceUsageRate(
      *t0_data, *t1_data, kIntervalDuration, kM1Timebase, std::nullopt);
  ASSERT_TRUE(rate);
  EXPECT_DOUBLE_EQ(kExpectedCPUUsagePerSecondPercent,
                   rate->cpu_time_per_second);
  EXPECT_DOUBLE_EQ(kExpectedInterruptWakeUpPerSecond,
                   rate->interrupt_wakeups_per_second);
  EXPECT_DOUBLE_EQ(kExpectedPlatformIdleWakeUpPerSecond,
                   rate->platform_idle_wakeups_per_second);
  EXPECT_DOUBLE_EQ(kExpectedBytesReadPerSecond, rate->bytesread_per_second);
  EXPECT_DOUBLE_EQ(kExpectedBytesWrittenPerSecond,
                   rate->byteswritten_per_second);
  EXPECT_DOUBLE_EQ(kExpectedGPUUsagePerSecondPercent,
                   rate->gpu_time_per_second);
  EXPECT_FALSE(rate->energy_impact_per_second.has_value());
  EXPECT_DOUBLE_EQ(kExpectedPowerNW, rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     rate->qos_time_per_second[i]);
  }
}

TEST(ResourceCoalitionMacTest, GetDataRate_WithEnergyImpact_Intel) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageRateTestData(kIntelTimebase);

  auto rate =
      GetCoalitionResourceUsageRate(*t0_data, *t1_data, kIntervalDuration,
                                    kIntelTimebase, kEnergyImpactCoefficients);
  ASSERT_TRUE(rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent, rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond, rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent, rate->gpu_time_per_second);
  ASSERT_TRUE(rate->energy_impact_per_second.has_value());
  EXPECT_EQ(271.2, rate->energy_impact_per_second.value());
  EXPECT_FLOAT_EQ(kExpectedPowerNW, rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     rate->qos_time_per_second[i]);
  }
}

TEST(ResourceCoalitionMacTest, GetDataRate_WithEnergyImpact_M1) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageRateTestData(kM1Timebase);

  auto rate =
      GetCoalitionResourceUsageRate(*t0_data, *t1_data, kIntervalDuration,
                                    kM1Timebase, kEnergyImpactCoefficients);
  ASSERT_TRUE(rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent, rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond, rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent, rate->gpu_time_per_second);
  ASSERT_TRUE(rate->energy_impact_per_second.has_value());
  EXPECT_EQ(271.2, rate->energy_impact_per_second.value());
  EXPECT_FLOAT_EQ(kExpectedPowerNW, rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     rate->qos_time_per_second[i]);
  }
}

namespace {

bool DataOverflowInvalidatesDiffImpl(
    std::unique_ptr<coalition_resource_usage> t0,
    std::unique_ptr<coalition_resource_usage> t1,
    uint64_t* field_to_overflow) {
  // Initialize all fields to a non zero value.
  ::memset(t0.get(), 1000, sizeof(coalition_resource_usage));
  ::memset(t1.get(), 1000, sizeof(coalition_resource_usage));
  *field_to_overflow = 0;
  t1->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
  return !GetCoalitionResourceUsageRate(*t0, *t1, kIntervalDuration,
                                        kIntelTimebase, std::nullopt)
              .has_value();
}

bool DataOverflowInvalidatesDiff(
    uint64_t coalition_resource_usage::*member_ptr) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();
  auto* ptr = &(t1_data.get()->*member_ptr);
  return DataOverflowInvalidatesDiffImpl(std::move(t0_data), std::move(t1_data),
                                         ptr);
}

bool DataOverflowInvalidatesDiff(
    uint64_t (
        coalition_resource_usage::*member_ptr)[COALITION_NUM_THREAD_QOS_TYPES],
    int index_to_check) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();
  auto* ptr = &(t1_data.get()->*member_ptr)[index_to_check];
  return DataOverflowInvalidatesDiffImpl(std::move(t0_data), std::move(t1_data),
                                         ptr);
}

}  // namespace

// If one of these tests fails then it means that overflows on a newly tracked
// coalition field aren't tracked properly in GetCoalitionResourceUsageRate().
TEST(ResourceCoalitionTests, Overflows) {
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::energy_billed_to_me));
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::tasks_started));
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::tasks_exited));
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::time_nonempty));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(&coalition_resource_usage::cpu_time));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::interrupt_wakeups));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::platform_idle_wakeups));
  EXPECT_TRUE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::bytesread));
  EXPECT_TRUE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::byteswritten));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(&coalition_resource_usage::gpu_time));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::cpu_time_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::cpu_time_billed_to_others));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(&coalition_resource_usage::energy));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_immediate_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_deferred_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_invalidated_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_metadata_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_immediate_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_deferred_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_invalidated_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::logical_metadata_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::energy_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::energy_billed_to_others));
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::cpu_ptime));
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_TRUE(DataOverflowInvalidatesDiff(
        &coalition_resource_usage::cpu_time_eqos, i));
  }
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::cpu_instructions));
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::cpu_cycles));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      &coalition_resource_usage::fs_metadata_writes));
  EXPECT_FALSE(
      DataOverflowInvalidatesDiff(&coalition_resource_usage::pm_writes));
}

}  // namespace power_metrics
