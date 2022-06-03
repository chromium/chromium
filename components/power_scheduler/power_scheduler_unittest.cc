// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_scheduler.h"

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/power_scheduler/power_scheduler_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_scheduler {

class TestPowerScheduler : public PowerScheduler {
 public:
  TestPowerScheduler(PowerModeArbiter* arbiter) : PowerScheduler(arbiter) {}

  base::CpuAffinityMode GetTargetCpuAffinity() {
    return PowerScheduler::GetTargetCpuAffinity();
  }

  SchedulingPolicyParams GetPolicy() { return GetPolicyForTesting(); }

  base::CpuAffinityMode last_enforced_affinity_mode() {
    return last_enforced_affinity_mode_;
  }

  void SetTaskRunner(scoped_refptr<base::TaskRunner> thread_pool_task_runner) {
    SetupTaskRunners(thread_pool_task_runner);
  }

  base::CpuAffinityMode last_enforced_affinity_mode_ =
      base::CpuAffinityMode::kDefault;

  void AdvanceCpuTime(base::TimeDelta cpu_time) { cpu_time_ += cpu_time; }

 protected:
  void EnforceCpuAffinityOnSequence() override {
    last_enforced_affinity_mode_ = GetEnforcedCpuAffinityForTesting();
  }

  base::TimeDelta GetProcessCpuTime() override { return cpu_time_; }

  base::TimeDelta cpu_time_;
};

class PowerSchedulerTest : public testing::Test {
 public:
  PowerSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scheduler_(&arbiter_) {
    scheduler_.SetTaskRunner(base::SequencedTaskRunnerHandle::Get());
  }
  ~PowerSchedulerTest() override = default;

  void SetPolicyAndExpect(SchedulingPolicy policy,
                          base::CpuAffinityMode affinity) {
    SchedulingPolicyParams params;
    params.policy = policy;
    SetPolicyAndExpect(params, affinity);
  }

  void SetPolicyAndExpect(SchedulingPolicyParams policy,
                          base::CpuAffinityMode affinity) {
    scheduler_.SetPolicy(policy);
    Expect(affinity);
  }

  void SetPowerModeAndExpect(PowerMode mode, base::CpuAffinityMode affinity) {
    SCOPED_TRACE(std::string(PowerModeToString(mode)) + ", " +
                 (affinity == base::CpuAffinityMode::kLittleCoresOnly
                      ? "kLittleCoresOnly"
                      : "kDefault"));
    scheduler_.OnPowerModeChanged(last_power_mode_, mode);
    last_power_mode_ = mode;
    Expect(affinity);
  }

  void Expect(base::CpuAffinityMode affinity) {
    task_environment_.RunUntilIdle();
    if (!base::HasBigCpuCores())
      affinity = base::CpuAffinityMode::kDefault;
    EXPECT_EQ(scheduler_.GetTargetCpuAffinity(), affinity);
    EXPECT_EQ(scheduler_.last_enforced_affinity_mode(), affinity);
  }

  void ExpectPolicy(SchedulingPolicy policy,
                    int min_time_in_mode_ms,
                    double min_cputime_ratio) {
    task_environment_.RunUntilIdle();
    if (!base::HasBigCpuCores()) {
      policy = SchedulingPolicy::kNone;
      min_time_in_mode_ms = 0;
      min_cputime_ratio = 0;
    }
    EXPECT_EQ(scheduler_.GetPolicy().policy, policy);
    EXPECT_EQ(scheduler_.GetPolicy().min_time_in_mode,
              base::Milliseconds(min_time_in_mode_ms));
    EXPECT_NEAR(scheduler_.GetPolicy().min_cputime_ratio, min_cputime_ratio,
                0.01);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  PowerModeArbiter arbiter_;
  TestPowerScheduler scheduler_;
  PowerMode last_power_mode_ = PowerMode::kMaxValue;
};

TEST_F(PowerSchedulerTest, NoPolicy) {
  EXPECT_EQ(scheduler_.GetTargetCpuAffinity(), base::CpuAffinityMode::kDefault);
  EXPECT_EQ(scheduler_.last_enforced_affinity_mode(),
            base::CpuAffinityMode::kDefault);

  SetPolicyAndExpect(SchedulingPolicy::kNone, base::CpuAffinityMode::kDefault);
}

TEST_F(PowerSchedulerTest, LittleCoresOnly) {
  SetPolicyAndExpect(SchedulingPolicy::kLittleCoresOnly,
                     base::CpuAffinityMode::kLittleCoresOnly);
}

TEST_F(PowerSchedulerTest, ThrottleIdle) {
  SetPolicyAndExpect(SchedulingPolicy::kThrottleIdle,
                     base::CpuAffinityMode::kDefault);

  SetPowerModeAndExpect(PowerMode::kAnimation, base::CpuAffinityMode::kDefault);
  SetPowerModeAndExpect(PowerMode::kIdle,
                        base::CpuAffinityMode::kLittleCoresOnly);
  SetPowerModeAndExpect(PowerMode::kAnimation, base::CpuAffinityMode::kDefault);
  SetPowerModeAndExpect(PowerMode::kNopAnimation,
                        base::CpuAffinityMode::kDefault);
  SetPowerModeAndExpect(PowerMode::kIdle,
                        base::CpuAffinityMode::kLittleCoresOnly);
}

TEST_F(PowerSchedulerTest, ThrottleIdleAndNopAnimation) {
  SetPolicyAndExpect(SchedulingPolicy::kThrottleIdleAndNopAnimation,
                     base::CpuAffinityMode::kDefault);

  SetPowerModeAndExpect(PowerMode::kAnimation, base::CpuAffinityMode::kDefault);
  SetPowerModeAndExpect(PowerMode::kIdle,
                        base::CpuAffinityMode::kLittleCoresOnly);
  SetPowerModeAndExpect(PowerMode::kAnimation, base::CpuAffinityMode::kDefault);
  SetPowerModeAndExpect(PowerMode::kNopAnimation,
                        base::CpuAffinityMode::kLittleCoresOnly);
  SetPowerModeAndExpect(PowerMode::kIdle,
                        base::CpuAffinityMode::kLittleCoresOnly);
}

TEST_F(PowerSchedulerTest, ThrottleIdleWithMinimums) {
  SchedulingPolicyParams params{SchedulingPolicy::kThrottleIdle,
                                base::Milliseconds(500), 0.5};
  SetPolicyAndExpect(params, base::CpuAffinityMode::kDefault);

  SetPowerModeAndExpect(PowerMode::kIdle, base::CpuAffinityMode::kDefault);
  // Advancing time without incrementing CPU - stay unthrottled.
  task_environment_.FastForwardBy(params.min_time_in_mode);
  Expect(base::CpuAffinityMode::kDefault);

  for (int i = 0; i < 3; i++) {
    SetPowerModeAndExpect(PowerMode::kAnimation,
                          base::CpuAffinityMode::kDefault);
    SetPowerModeAndExpect(PowerMode::kIdle, base::CpuAffinityMode::kDefault);
    // Advancing time with incrementing CPU above min - throttle.
    scheduler_.AdvanceCpuTime(base::Milliseconds(300));
    task_environment_.FastForwardBy(params.min_time_in_mode);
    Expect(base::CpuAffinityMode::kLittleCoresOnly);

    // Reset to default occurs immediately.
    SetPowerModeAndExpect(PowerMode::kAnimation,
                          base::CpuAffinityMode::kDefault);
    SetPowerModeAndExpect(PowerMode::kIdle, base::CpuAffinityMode::kDefault);
    // Advancing time with incrementing CPU below min - stay unthrottled.
    scheduler_.AdvanceCpuTime(base::Milliseconds(100));
    task_environment_.FastForwardBy(params.min_time_in_mode);
    Expect(base::CpuAffinityMode::kDefault);
  }

  // Timer resets after each change of modes.
  for (int i = 0; i < 3; i++) {
    SetPowerModeAndExpect(PowerMode::kAnimation,
                          base::CpuAffinityMode::kDefault);
    SetPowerModeAndExpect(PowerMode::kIdle, base::CpuAffinityMode::kDefault);
    scheduler_.AdvanceCpuTime(base::Milliseconds(300));
    task_environment_.FastForwardBy(params.min_time_in_mode / 2);
    Expect(base::CpuAffinityMode::kDefault);
  }
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListEmpty) {
  scheduler_.InitializePolicyFromFeatureList();
  EXPECT_EQ(scheduler_.GetPolicy().policy, SchedulingPolicy::kNone);
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListLittleOnly) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kCpuAffinityRestrictToLittleCores);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kLittleCoresOnly, 0, 0);
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListIdle) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kPowerSchedulerThrottleIdle);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdle, 0, 0);
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListIdleAndNopAnimation) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(
      features::kPowerSchedulerThrottleIdleAndNopAnimation);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdleAndNopAnimation, 0, 0);
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListWebViewLittleOnly) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kWebViewCpuAffinityRestrictToLittleCores);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kLittleCoresOnly, 0, 0);
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListWebViewIdle) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kWebViewPowerSchedulerThrottleIdle);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdle, 0, 0);
}

TEST_F(PowerSchedulerTest, InitializePolicyFromFeatureListPowerScheduler) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kPowerScheduler);
  scheduler_.InitializePolicyFromFeatureList();
  // No field trial params, use built-in defaults.
  ExpectPolicy(SchedulingPolicy::kThrottleIdleAndNopAnimation, 500, 0.5);
}

TEST_F(PowerSchedulerTest,
       InitializePolicyFromFeatureListPowerSchedulerWithParams) {
  base::test::ScopedFeatureList list;
  base::FieldTrialParams params;
  params["policy"] = "kThrottleIdle";
  params["min_time_in_mode_ms"] = "1000";
  params["min_cputime_ratio"] = "1.0";
  params["include_charging"] = "false";
  list.InitAndEnableFeatureWithParameters(features::kPowerScheduler, params);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdle, 1000, 1.0);
}

TEST_F(PowerSchedulerTest,
       InitializePolicyFromFeatureListPowerSchedulerWithParamsIncludeCharging) {
  base::test::ScopedFeatureList list;
  base::FieldTrialParams params;
  params["policy"] = "kThrottleIdle";
  params["min_time_in_mode_ms"] = "1000";
  params["min_cputime_ratio"] = "1.0";
  params["include_charging"] = "true";
  list.InitAndEnableFeatureWithParameters(features::kPowerScheduler, params);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdle, 1000, 1.0);

  if (base::HasBigCpuCores()) {
    // |include_charging| disables kCharging modes in the arbiter.
    std::unique_ptr<PowerModeVoter> voter = arbiter_.NewVoter("test");
    voter->VoteFor(PowerMode::kCharging);
    EXPECT_EQ(arbiter_.GetActiveModeForTesting(), PowerMode::kIdle);
  }
}

TEST_F(PowerSchedulerTest,
       InitializePolicyFromFeatureListPowerSchedulerWithParamsAllowedRenderer) {
  base::test::ScopedCommandLine original_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("type", "renderer");
  base::test::ScopedFeatureList list;
  base::FieldTrialParams params;
  params["policy"] = "kThrottleIdle";
  params["min_time_in_mode_ms"] = "1000";
  params["min_cputime_ratio"] = "1.0";
  params["process_types"] = "browser,renderer";
  list.InitAndEnableFeatureWithParameters(features::kPowerScheduler, params);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdle, 1000, 1.0);
}

TEST_F(PowerSchedulerTest,
       InitializePolicyFromFeatureListPowerSchedulerWithParamsAllowedBrowser) {
  base::test::ScopedFeatureList list;
  base::FieldTrialParams params;
  params["policy"] = "kThrottleIdle";
  params["min_time_in_mode_ms"] = "1000";
  params["min_cputime_ratio"] = "1.0";
  params["process_types"] = "browser,renderer";
  list.InitAndEnableFeatureWithParameters(features::kPowerScheduler, params);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kThrottleIdle, 1000, 1.0);
}

TEST_F(PowerSchedulerTest,
       InitializePolicyFromFeatureListPowerSchedulerWithParamsBlockedRenderer) {
  base::test::ScopedCommandLine original_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("type", "renderer");
  base::test::ScopedFeatureList list;
  base::FieldTrialParams params;
  params["policy"] = "kThrottleIdle";
  params["min_time_in_mode_ms"] = "1000";
  params["min_cputime_ratio"] = "1.0";
  params["process_types"] = "browser";
  list.InitAndEnableFeatureWithParameters(features::kPowerScheduler, params);
  scheduler_.InitializePolicyFromFeatureList();
  ExpectPolicy(SchedulingPolicy::kNone, 0, 0);
}

}  // namespace power_scheduler