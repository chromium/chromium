// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_scheduler.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/cpu.h"
#include "base/cpu_affinity_posix.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/power_scheduler/power_scheduler_features.h"

namespace power_scheduler {
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with base::CpuAffinityMode and CpuAffinityMode in enums.xml.
enum class CpuAffinityModeForUma {
  kDefault = 0,
  kLittleCoresOnly = 1,
  kBigCoresOnly = 2,
  kBiggerCoresOnly = 3,
  kMaxValue = kBiggerCoresOnly,
};

CpuAffinityModeForUma GetCpuAffinityModeForUma(base::CpuAffinityMode affinity) {
  switch (affinity) {
    case base::CpuAffinityMode::kDefault:
      return CpuAffinityModeForUma::kDefault;
    case base::CpuAffinityMode::kLittleCoresOnly:
      return CpuAffinityModeForUma::kLittleCoresOnly;
    case base::CpuAffinityMode::kBigCoresOnly:
      return CpuAffinityModeForUma::kBigCoresOnly;
    case base::CpuAffinityMode::kBiggerCoresOnly:
      return CpuAffinityModeForUma::kBiggerCoresOnly;
  }
}

perfetto::StaticString TraceEventNameForAffinityMode(
    base::CpuAffinityMode affinity) {
  switch (affinity) {
    case base::CpuAffinityMode::kDefault:
      return "ApplyCpuAffinityModeDefault";
    case base::CpuAffinityMode::kLittleCoresOnly:
      return "ApplyCpuAffinityModeLittleCoresOnly";
    case base::CpuAffinityMode::kBigCoresOnly:
      return "ApplyCpuAffinityModeBigCoresOnly";
    case base::CpuAffinityMode::kBiggerCoresOnly:
      return "ApplyCpuAffinityModeBiggerCoresOnly";
  }
}

void ApplyProcessCpuAffinityMode(base::CpuAffinityMode affinity) {
  TRACE_EVENT("power", TraceEventNameForAffinityMode(affinity));

  // Restrict affinity of all existing threads of the current process. The
  // affinity is inherited by any subsequently created thread. Other threads may
  // already exist even during early startup (e.g. Java threads like
  // RenderThread), so setting the affinity only for the current thread is not
  // enough here.
  bool success = base::SetProcessCpuAffinityMode(
      base::GetCurrentProcessHandle(), affinity);

  base::UmaHistogramBoolean(
      "Power.CpuAffinityExperiments.ProcessAffinityUpdateSuccess", success);
  if (success) {
    base::UmaHistogramEnumeration(
        "Power.CpuAffinityExperiments.ProcessAffinityMode",
        GetCpuAffinityModeForUma(affinity));
  }
}

bool CpuAffinityApplicable() {
  // For now, affinity modes only have an effect on big.LITTLE architectures.
  return base::HasBigCpuCores();
}

// Default policy params for the PowerScheduler feature. Please update the
// comment in power_scheduler_features.cc when changing these defaults.
static constexpr SchedulingPolicyParams kDefaultParams{
    SchedulingPolicy::kThrottleIdleAndNopAnimation, base::Milliseconds(500),
    0.5f};

// Keys/values for the field trial params.
static const char kPolicyKey[] = "policy";
static const char kPolicyLittleCoresOnly[] = "kLittleCoresOnly";
static const char kPolicyThrottleIdle[] = "kThrottleIdle";
static const char kPolicyThrottleIdleAndNopAnimation[] =
    "kThrottleIdleAndNopAnimation";
static const char kMinTimeInModeMsKey[] = "min_time_in_mode_ms";
static const char kMinCputimeRatioKey[] = "min_cputime_ratio";
static const char kProcessTypesKey[] = "process_types";
static const char kIncludeChargingKey[] = "include_charging";

}  // anonymous namespace

PowerScheduler::PowerScheduler(PowerModeArbiter* arbiter)
    : arbiter_(arbiter),
      process_metrics_(base::ProcessMetrics::CreateCurrentProcessMetrics()) {
  DETACH_FROM_SEQUENCE(thread_pool_checker_);
}

PowerScheduler::~PowerScheduler() = default;

// static
PowerScheduler* PowerScheduler::GetInstance() {
  static base::NoDestructor<PowerScheduler> instance(
      PowerModeArbiter::GetInstance());
  return instance.get();
}

void PowerScheduler::WillProcessTask(const base::PendingTask& pending_task,
                                     bool was_blocked_or_low_priority) {}

void PowerScheduler::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  ++task_counter_;
  if (task_counter_ == kUpdateAfterEveryNTasks) {
    thread_pool_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerScheduler::EnforceCpuAffinityOnSequence,
                                  base::Unretained(this)  // never destroyed.
                                  ));
    task_counter_ = 0;
  }
}

void PowerScheduler::OnPowerModeChanged(PowerMode old_mode,
                                        PowerMode new_mode) {
  TRACE_EVENT2("power", "PowerScheduler::OnPowerModeChanged", "old_mode",
               PowerModeToString(old_mode), "new_mode",
               PowerModeToString(new_mode));

  OnPowerModeChangedOnSequence(old_mode, new_mode);
}

void PowerScheduler::Setup() {
  // The setup should be called once from the main thread. In single-process
  // mode, it may later be called on other threads (which should be ignored).
  if (did_call_setup_)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  did_call_setup_ = true;
  SetupTaskRunners(base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}

void PowerScheduler::SetupTaskRunners(
    scoped_refptr<base::TaskRunner> thread_pool_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  main_thread_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  thread_pool_task_runner_ = thread_pool_task_runner;

  if (pending_policy_.policy == SchedulingPolicy::kNone)
    return;

  thread_pool_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PowerScheduler::SetupPolicyOnSequence,
                                base::Unretained(this),  // never destroyed.
                                pending_policy_));
  pending_policy_ = SchedulingPolicyParams();
}

base::TimeDelta PowerScheduler::GetProcessCpuTime() {
  return process_metrics_->GetCumulativeCPUUsage();
}

void PowerScheduler::InitializePolicyFromFeatureList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  // To preserve earlier behavior of the field trials, check for the legacy
  // features before checking if throttling is supported.
  if (base::FeatureList::IsEnabled(
          features::kCpuAffinityRestrictToLittleCores)) {
    SetPolicy(SchedulingPolicy::kLittleCoresOnly);
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kPowerSchedulerThrottleIdleAndNopAnimation)) {
    SetPolicy(SchedulingPolicy::kThrottleIdleAndNopAnimation);
    return;
  }
  if (base::FeatureList::IsEnabled(features::kPowerSchedulerThrottleIdle)) {
    SetPolicy(SchedulingPolicy::kThrottleIdle);
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kWebViewCpuAffinityRestrictToLittleCores)) {
    SetPolicy(SchedulingPolicy::kLittleCoresOnly);
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kWebViewPowerSchedulerThrottleIdle)) {
    SetPolicy(SchedulingPolicy::kThrottleIdle);
    return;
  }

  // Only check for the new feature after checking that throttling is supported.
  if (!CpuAffinityApplicable())
    return;

  if (!base::FeatureList::IsEnabled(features::kPowerScheduler))
    return;

  SchedulingPolicyParams params = kDefaultParams;

  std::map<std::string, std::string> field_trial_params;
  if (base::GetFieldTrialParamsByFeature(features::kPowerScheduler,
                                         &field_trial_params)) {
    auto policy_it = field_trial_params.find(kPolicyKey);
    if (policy_it != field_trial_params.end()) {
      if (policy_it->second == kPolicyLittleCoresOnly) {
        params.policy = SchedulingPolicy::kLittleCoresOnly;
      } else if (policy_it->second == kPolicyThrottleIdle) {
        params.policy = SchedulingPolicy::kThrottleIdle;
      } else if (policy_it->second == kPolicyThrottleIdleAndNopAnimation) {
        params.policy = SchedulingPolicy::kThrottleIdleAndNopAnimation;
      }
    }

    int min_time_ms = 0;
    if (base::StringToInt(field_trial_params[kMinTimeInModeMsKey],
                          &min_time_ms)) {
      params.min_time_in_mode = base::Milliseconds(min_time_ms);
    }

    double min_cputime_ratio = 0;
    if (base::StringToDouble(field_trial_params[kMinCputimeRatioKey],
                             &min_cputime_ratio)) {
      params.min_cputime_ratio = min_cputime_ratio;
    }

    // If there is a process type allowlist, check if the current process's type
    // is in it. Otherwise don't enable power scheduling.
    const std::string& process_types = field_trial_params[kProcessTypesKey];
    if (!process_types.empty()) {
      std::vector<std::string> split = base::SplitString(
          process_types, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
          base::SplitResult::SPLIT_WANT_NONEMPTY);

      std::string process_type =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type");

      if (process_type.empty()) {
        if (!base::Contains(split, "browser"))
          return;
      } else if (!base::Contains(split, process_type)) {
        return;
      }
    }

    bool include_charging = field_trial_params[kIncludeChargingKey] == "true";
    if (include_charging)
      arbiter_->SetChargingModeEnabled(false);
  }

  SetPolicy(params);
}

void PowerScheduler::SetPolicy(SchedulingPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  SchedulingPolicyParams params;
  params.policy = policy;
  SetPolicy(params);
}

void PowerScheduler::SetPolicy(SchedulingPolicyParams policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_checker_);

  if (!CpuAffinityApplicable())
    return;

  // Little-only policy only makes sense without minimum times.
  if (policy.policy == SchedulingPolicy::kLittleCoresOnly) {
    policy.min_time_in_mode = base::TimeDelta();
    policy.min_cputime_ratio = 0;
  }

  // Set up the power affinity observer and apply the policy if it's already
  // possible. Otherwise it will be set up after thread initialization via
  // Setup() (see app/content_main_runner_impl.cc and child/child_process.cc).
  if (thread_pool_task_runner_) {
    thread_pool_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerScheduler::SetupPolicyOnSequence,
                                  base::Unretained(this),  // never destroyed.
                                  policy));
  } else {
    pending_policy_ = policy;
  }
}

void PowerScheduler::SetupPolicyOnSequence(SchedulingPolicyParams policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  // Reset the power mode in case it contains an obsolete value.
  current_power_mode_ = PowerMode::kMaxValue;
  current_policy_ = policy;

  bool needs_power_observer =
      current_policy_.policy == SchedulingPolicy::kThrottleIdle ||
      current_policy_.policy == SchedulingPolicy::kThrottleIdleAndNopAnimation;

  if (needs_power_observer && !power_observer_registered_) {
    arbiter_->AddObserver(this);
    power_observer_registered_ = true;
  } else if (!needs_power_observer && power_observer_registered_) {
    arbiter_->RemoveObserver(this);
    power_observer_registered_ = false;
  }

  ApplyPolicyOnSequence();
}

void PowerScheduler::OnPowerModeChangedOnSequence(PowerMode old_mode,
                                                  PowerMode new_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  current_power_mode_ = new_mode;
  ApplyPolicyOnSequence();
}

base::CpuAffinityMode PowerScheduler::GetTargetCpuAffinity() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  bool is_throttleable_mode = false;
  switch (current_policy_.policy) {
    case SchedulingPolicy::kNone:
      break;
    case SchedulingPolicy::kLittleCoresOnly:
      is_throttleable_mode = true;
      break;
    case SchedulingPolicy::kThrottleIdle:
      is_throttleable_mode = current_power_mode_ == PowerMode::kIdle ||
                             current_power_mode_ == PowerMode::kBackground;
      break;
    case SchedulingPolicy::kThrottleIdleAndNopAnimation:
      is_throttleable_mode = current_power_mode_ == PowerMode::kIdle ||
                             current_power_mode_ == PowerMode::kBackground ||
                             current_power_mode_ == PowerMode::kNopAnimation;
      break;
  }

  bool currently_throttling =
      enforced_affinity_ == base::CpuAffinityMode::kLittleCoresOnly;

  if (is_throttleable_mode == currently_throttling) {
    time_entered_throttleable_mode_ = base::TimeTicks();
    cputime_entered_throttleable_mode_ = base::TimeDelta();
    return enforced_affinity_;
  }

  // If we are in a throttleable mode, check if we've been in a throttleable
  // mode for long enough and consumed enough CPU. Otherwise, don't change
  // anything (yet) and schedule a follow-up check if needed.
  if (is_throttleable_mode && current_policy_.min_time_in_mode.is_positive()) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta cumulative_cpu = GetProcessCpuTime();

    if (time_entered_throttleable_mode_.is_null()) {
      time_entered_throttleable_mode_ = now;
      cputime_entered_throttleable_mode_ = cumulative_cpu;
      thread_pool_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PowerScheduler::ApplyPolicyOnSequence,
                         base::Unretained(this)),  // never destroyed.
          current_policy_.min_time_in_mode);
      return enforced_affinity_;
    } else {
      bool minimums_exceeded = false;
      base::TimeDelta time_elapsed = now - time_entered_throttleable_mode_;
      base::TimeDelta cputime_elapsed =
          cumulative_cpu - cputime_entered_throttleable_mode_;
      minimums_exceeded =
          time_elapsed >= current_policy_.min_time_in_mode &&
          cputime_elapsed >= time_elapsed * current_policy_.min_cputime_ratio;

      if (!minimums_exceeded) {
        TRACE_EVENT_INSTANT2("power", "PowerScheduler.MinimumsNotExceeded",
                             TRACE_EVENT_SCOPE_THREAD, "time_elapsed_ms",
                             time_elapsed.InMilliseconds(),
                             "cputime_elapsed_ms",
                             cputime_elapsed.InMilliseconds());
        return enforced_affinity_;
      } else {
        TRACE_EVENT_INSTANT2("power", "PowerScheduler.MinimumsExceeded",
                             TRACE_EVENT_SCOPE_THREAD, "time_elapsed_ms",
                             time_elapsed.InMilliseconds(),
                             "cputime_elapsed_ms",
                             cputime_elapsed.InMilliseconds());
      }
    }
  } else {
    time_entered_throttleable_mode_ = base::TimeTicks();
    cputime_entered_throttleable_mode_ = base::TimeDelta();
  }

  return is_throttleable_mode ? base::CpuAffinityMode::kLittleCoresOnly
                              : base::CpuAffinityMode::kDefault;
}

void PowerScheduler::ApplyPolicyOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);
  TRACE_EVENT0("power", "PowerScheduler::ApplyPolicyOnSequence");

  base::CpuAffinityMode target_affinity = GetTargetCpuAffinity();
  if (target_affinity == enforced_affinity_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  if (target_affinity == base::CpuAffinityMode::kDefault &&
      !enforced_affinity_setup_time_.is_null()) {
    auto throttling_duration = now - enforced_affinity_setup_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Power.PowerScheduler.ThrottlingDuration",
                               throttling_duration, base::Milliseconds(1),
                               base::Minutes(10), 100);

    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.PowerScheduler.ThrottlingDurationPerCpuAffinityMode",
        GetCpuAffinityModeForUma(enforced_affinity_),
        throttling_duration.InMicroseconds(),
        base::Time::kMicrosecondsPerMillisecond);
  }

  if (target_affinity == base::CpuAffinityMode::kLittleCoresOnly) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("power", "PowerScheduler.LittleCoresOnly",
                                      this);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0("power", "PowerScheduler.LittleCoresOnly",
                                    this);
  }

  enforced_affinity_setup_time_ = now;
  enforced_affinity_ = target_affinity;
  TRACE_EVENT_INSTANT1("power", "PowerScheduler.NewAffinity",
                       TRACE_EVENT_SCOPE_THREAD, "enforced_affinity",
                       enforced_affinity_);
  EnforceCpuAffinityOnSequence();
}

void PowerScheduler::EnforceCpuAffinityOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_checker_);

  if (enforced_affinity_ != base::CurrentThreadCpuAffinityMode())
    ApplyProcessCpuAffinityMode(enforced_affinity_);

  // Android system may reset a non-default affinity setting, so we need to
  // check periodically if we need to re-apply it.
  bool mode_needs_periodic_enforcement =
      enforced_affinity_ != base::CpuAffinityMode::kDefault;

  if (mode_needs_periodic_enforcement && !task_observer_registered_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](PowerScheduler* scheduler) {
                         base::CurrentThread::Get()->AddTaskObserver(scheduler);
                       },
                       base::Unretained(this)));  // never deleted.
    task_observer_registered_ = true;
  } else if (!mode_needs_periodic_enforcement && task_observer_registered_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](PowerScheduler* scheduler) {
              base::CurrentThread::Get()->RemoveTaskObserver(scheduler);
              scheduler->task_counter_ = 0;
            },
            base::Unretained(this)));  // never deleted.
    task_observer_registered_ = false;
  }
}

}  // namespace power_scheduler
