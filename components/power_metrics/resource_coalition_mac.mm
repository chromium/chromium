// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/power_metrics/resource_coalition_mac.h"

#include <libproc.h>

#include "base/check_op.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "components/power_metrics/mach_time_mac.h"

extern "C" int coalition_info_resource_usage(
    uint64_t cid,
    struct coalition_resource_usage* cru,
    size_t sz);

namespace power_metrics {

std::optional<uint64_t> GetProcessCoalitionId(base::ProcessId pid) {
  proc_pidcoalitioninfo coalition_info = {};
  int res = proc_pidinfo(pid, PROC_PIDCOALITIONINFO, 0, &coalition_info,
                         sizeof(coalition_info));

  if (res != sizeof(coalition_info))
    return std::nullopt;

  return coalition_info.coalition_id[COALITION_TYPE_RESOURCE];
}

std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsage(
    int64_t coalition_id) {
  auto cru = std::make_unique<coalition_resource_usage>();
  uint64_t res = coalition_info_resource_usage(
      coalition_id, cru.get(), sizeof(coalition_resource_usage));
  if (res == 0U)
    return cru;
  return nullptr;
}

coalition_resource_usage GetCoalitionResourceUsageDifference(
    const coalition_resource_usage& left,
    const coalition_resource_usage& right) {
  DCHECK_GE(left.tasks_started, right.tasks_started);
  DCHECK_GE(left.tasks_exited, right.tasks_exited);
  DCHECK_GE(left.time_nonempty, right.time_nonempty);
  DCHECK_GE(left.cpu_time, right.cpu_time);
  DCHECK_GE(left.interrupt_wakeups, right.interrupt_wakeups);
  DCHECK_GE(left.platform_idle_wakeups, right.platform_idle_wakeups);
  DCHECK_GE(left.bytesread, right.bytesread);
  DCHECK_GE(left.byteswritten, right.byteswritten);
  DCHECK_GE(left.gpu_time, right.gpu_time);
  DCHECK_GE(left.cpu_time_billed_to_me, right.cpu_time_billed_to_me);
  DCHECK_GE(left.cpu_time_billed_to_others, right.cpu_time_billed_to_others);
  DCHECK_GE(left.energy, right.energy);
  DCHECK_GE(left.logical_immediate_writes, right.logical_immediate_writes);
  DCHECK_GE(left.logical_deferred_writes, right.logical_deferred_writes);
  DCHECK_GE(left.logical_invalidated_writes, right.logical_invalidated_writes);
  DCHECK_GE(left.logical_metadata_writes, right.logical_metadata_writes);
  DCHECK_GE(left.logical_immediate_writes_to_external,
            right.logical_immediate_writes_to_external);
  DCHECK_GE(left.logical_deferred_writes_to_external,
            right.logical_deferred_writes_to_external);
  DCHECK_GE(left.logical_invalidated_writes_to_external,
            right.logical_invalidated_writes_to_external);
  DCHECK_GE(left.logical_metadata_writes_to_external,
            right.logical_metadata_writes_to_external);
  DCHECK_GE(left.energy_billed_to_me, right.energy_billed_to_me);
  DCHECK_GE(left.energy_billed_to_others, right.energy_billed_to_others);
  DCHECK_GE(left.cpu_ptime, right.cpu_ptime);
  DCHECK_GE(left.cpu_instructions, right.cpu_instructions);
  DCHECK_GE(left.cpu_cycles, right.cpu_cycles);
  DCHECK_GE(left.fs_metadata_writes, right.fs_metadata_writes);
  DCHECK_GE(left.pm_writes, right.pm_writes);

  coalition_resource_usage ret;

  ret.tasks_started = left.tasks_started - right.tasks_started;
  ret.tasks_exited = left.tasks_exited - right.tasks_exited;
  ret.time_nonempty = left.time_nonempty - right.time_nonempty;
  ret.cpu_time = left.cpu_time - right.cpu_time;
  ret.interrupt_wakeups = left.interrupt_wakeups - right.interrupt_wakeups;
  ret.platform_idle_wakeups =
      left.platform_idle_wakeups - right.platform_idle_wakeups;
  ret.bytesread = left.bytesread - right.bytesread;
  ret.byteswritten = left.byteswritten - right.byteswritten;
  ret.gpu_time = left.gpu_time - right.gpu_time;
  ret.cpu_time_billed_to_me =
      left.cpu_time_billed_to_me - right.cpu_time_billed_to_me;
  ret.cpu_time_billed_to_others =
      left.cpu_time_billed_to_others - right.cpu_time_billed_to_others;
  ret.energy = left.energy - right.energy;
  ret.logical_immediate_writes =
      left.logical_immediate_writes - right.logical_immediate_writes;
  ret.logical_deferred_writes =
      left.logical_deferred_writes - right.logical_deferred_writes;
  ret.logical_invalidated_writes =
      left.logical_invalidated_writes - right.logical_invalidated_writes;
  ret.logical_metadata_writes =
      left.logical_metadata_writes - right.logical_metadata_writes;
  ret.logical_immediate_writes_to_external =
      left.logical_immediate_writes_to_external -
      right.logical_immediate_writes_to_external;
  ret.logical_deferred_writes_to_external =
      left.logical_deferred_writes_to_external -
      right.logical_deferred_writes_to_external;
  ret.logical_invalidated_writes_to_external =
      left.logical_invalidated_writes_to_external -
      right.logical_invalidated_writes_to_external;
  ret.logical_metadata_writes_to_external =
      left.logical_metadata_writes_to_external -
      right.logical_metadata_writes_to_external;
  ret.energy_billed_to_me =
      left.energy_billed_to_me - right.energy_billed_to_me;
  ret.energy_billed_to_others =
      left.energy_billed_to_others - right.energy_billed_to_others;
  ret.cpu_ptime = left.cpu_ptime - right.cpu_ptime;

  DCHECK_EQ(left.cpu_time_eqos_len,
            static_cast<uint64_t>(COALITION_NUM_THREAD_QOS_TYPES));
  DCHECK_EQ(right.cpu_time_eqos_len,
            static_cast<uint64_t>(COALITION_NUM_THREAD_QOS_TYPES));

  ret.cpu_time_eqos_len = left.cpu_time_eqos_len;
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    if (right.cpu_time_eqos[i] > left.cpu_time_eqos[i]) {
      // TODO(fdoray): Investigate why this happens. In the meantime, pretend
      // that there was no CPU time at this QoS.
      ret.cpu_time_eqos[i] = 0;
    } else {
      ret.cpu_time_eqos[i] = left.cpu_time_eqos[i] - right.cpu_time_eqos[i];
    }
  }

  ret.cpu_instructions = left.cpu_instructions - right.cpu_instructions;
  ret.cpu_cycles = left.cpu_cycles - right.cpu_cycles;
  ret.fs_metadata_writes = left.fs_metadata_writes - right.fs_metadata_writes;
  ret.pm_writes = left.pm_writes - right.pm_writes;

  return ret;
}

std::optional<CoalitionResourceUsageRate> GetCoalitionResourceUsageRate(
    const coalition_resource_usage& begin,
    const coalition_resource_usage& end,
    base::TimeDelta interval_duration,
    mach_timebase_info_data_t timebase,
    std::optional<EnergyImpactCoefficients> energy_impact_coefficients) {
  // Validate that |end| >= |begin|.
  bool end_greater_or_equal_begin =
      std::tie(end.cpu_time, end.interrupt_wakeups, end.platform_idle_wakeups,
               end.bytesread, end.byteswritten, end.gpu_time, end.energy) >=
      std::tie(begin.cpu_time, begin.interrupt_wakeups,
               begin.platform_idle_wakeups, begin.bytesread, begin.byteswritten,
               begin.gpu_time, begin.energy);
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    if (end.cpu_time_eqos[i] < begin.cpu_time_eqos[i])
      end_greater_or_equal_begin = false;
  }
  if (!end_greater_or_equal_begin)
    return std::nullopt;

  auto get_rate_per_second = [&interval_duration](uint64_t begin,
                                                  uint64_t end) -> double {
    DCHECK_GE(end, begin);
    uint64_t diff = end - begin;
    return diff / interval_duration.InSecondsF();
  };

  auto get_time_rate_per_second = [&interval_duration, &timebase](
                                      uint64_t begin, uint64_t end) -> double {
    DCHECK_GE(end, begin);
    // Compute the delta in s, being careful to avoid truncation due to integral
    // division.
    double delta_sample_s =
        power_metrics::MachTimeToNs(end - begin, timebase) /
        static_cast<double>(base::Time::kNanosecondsPerSecond);
    return delta_sample_s / interval_duration.InSecondsF();
  };

  CoalitionResourceUsageRate result;

  result.cpu_time_per_second =
      get_time_rate_per_second(begin.cpu_time, end.cpu_time);
  result.interrupt_wakeups_per_second =
      get_rate_per_second(begin.interrupt_wakeups, end.interrupt_wakeups);
  result.platform_idle_wakeups_per_second = get_rate_per_second(
      begin.platform_idle_wakeups, end.platform_idle_wakeups);
  result.bytesread_per_second =
      get_rate_per_second(begin.bytesread, end.bytesread);
  result.byteswritten_per_second =
      get_rate_per_second(begin.byteswritten, end.byteswritten);
  result.gpu_time_per_second =
      get_time_rate_per_second(begin.gpu_time, end.gpu_time);
  result.power_nw = get_rate_per_second(begin.energy, end.energy);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    result.qos_time_per_second[i] =
        get_time_rate_per_second(begin.cpu_time_eqos[i], end.cpu_time_eqos[i]);
  }

  if (energy_impact_coefficients.has_value()) {
    result.energy_impact_per_second =
        (ComputeEnergyImpactForResourceUsage(
             end, energy_impact_coefficients.value(), timebase) -
         ComputeEnergyImpactForResourceUsage(
             begin, energy_impact_coefficients.value(), timebase)) /
        interval_duration.InSecondsF();
  }

  return result;
}

}  // power_metrics
