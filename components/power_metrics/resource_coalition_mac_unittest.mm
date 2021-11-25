// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/resource_coalition_mac.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace power_metrics {

namespace {

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

}  // namespace

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

}  // namespace power_metrics
