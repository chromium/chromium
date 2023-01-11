// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_system_memory_pressure_evaluator.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

#define MAKE_SURE_THREAD(callback, ...)                                 \
  if (!task_runner_->BelongsToCurrentThread()) {                        \
    task_runner_->PostTask(                                             \
        FROM_HERE,                                                      \
        base::BindOnce(&CastSystemMemoryPressureEvaluator::callback,    \
                       weak_ptr_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                             \
  }

namespace chromecast {
namespace {

// Memory thresholds (as fraction of total memory) for memory pressure levels.
// See more detailed description of pressure heuristic in PollPressureLevel.
// TODO(halliwell): tune thresholds based on data.
constexpr float kCriticalMemoryFraction = 0.25f;
constexpr float kModerateMemoryFraction = 0.4f;

// Default Relaxed memory thresholds selectively applied to a few apps.
constexpr float kRelaxedCriticalMemoryFraction = 0.1f;
constexpr float kRelaxedModerateMemoryFraction = 0.2f;

// Memory thresholds in MB for the simple heuristic based on 'free' memory.
constexpr int kCriticalFreeMemoryKB = 20 * 1024;
constexpr int kModerateFreeMemoryKB = 30 * 1024;

constexpr int kPollingIntervalMS = 5000;

int GetSystemReservedKb() {
  int rtn_kb_ = 0;
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  base::StringToInt(
      command_line->GetSwitchValueASCII(switches::kMemPressureSystemReservedKb),
      &rtn_kb_);
  DCHECK(rtn_kb_ >= 0);
  return std::max(rtn_kb_, 0);
}

}  // namespace

CastSystemMemoryPressureEvaluator::CastSystemMemoryPressureEvaluator(
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      critical_memory_fraction_command_line_(
          GetSwitchValueDouble(switches::kCastMemoryPressureCriticalFraction,
                               -1.0f)),
      moderate_memory_fraction_command_line_(
          GetSwitchValueDouble(switches::kCastMemoryPressureModerateFraction,
                               -1.0f)),
      system_reserved_kb_(GetSystemReservedKb()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      weak_ptr_factory_(this) {
  relaxed_critical_memory_fraction_ = kRelaxedCriticalMemoryFraction;
  relaxed_moderate_memory_fraction_ = kRelaxedModerateMemoryFraction;
  critical_memory_fraction_ = critical_memory_fraction_command_line_;
  moderate_memory_fraction_ = moderate_memory_fraction_command_line_;
  // If the fractions from command line parameters are invalid they are subject
  // to adjustment.
  AdjustMemoryFractions(false);
  PollPressureLevel();
}

CastSystemMemoryPressureEvaluator::~CastSystemMemoryPressureEvaluator() =
    default;

void CastSystemMemoryPressureEvaluator::PollPressureLevel() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::MemoryPressureListener::MemoryPressureLevel level =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  base::SystemMemoryInfoKB info;
  if (!base::GetSystemMemoryInfo(&info)) {
    LOG(ERROR) << "GetSystemMemoryInfo failed";
  } else if (system_reserved_kb_ != 0 || info.available != 0) {
    // Preferred memory pressure heuristic:
    // 1. Use /proc/meminfo's MemAvailable if possible, fall back to estimate
    // of free + buffers + cached otherwise.
    const int total_available = (info.available != 0)
                                    ? info.available
                                    : (info.free + info.buffers + info.cached);

    // 2. Allow some memory to be 'reserved' on command line.
    const int available = total_available - system_reserved_kb_;
    const int total = info.total - system_reserved_kb_;
    DCHECK_GT(total, 0);
    const float ratio = available / static_cast<float>(total);

    if (ratio < critical_memory_fraction_)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    else if (ratio < moderate_memory_fraction_)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else {
    // Backup method purely using 'free' memory.  It may generate more
    // pressure events than necessary, since more memory may actually be free.
    if (info.free < kCriticalFreeMemoryKB)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    else if (info.free < kModerateFreeMemoryKB)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  UpdateMemoryPressureLevel(level);

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastSystemMemoryPressureEvaluator::PollPressureLevel,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kPollingIntervalMS));
}

void CastSystemMemoryPressureEvaluator::UpdateMemoryPressureLevel(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto old_vote = current_vote();
  SetCurrentVote(new_level);

  SendCurrentVote(/* notify = */ current_vote() !=
                  base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  if (old_vote == current_vote())
    return;

  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
      "Memory.Pressure.LevelChange", new_level);
}

void CastSystemMemoryPressureEvaluator::AdjustMemoryFractions(bool relax) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (critical_memory_fraction_command_line_ < 0) {
    critical_memory_fraction_ =
        relax ? relaxed_critical_memory_fraction_ : kCriticalMemoryFraction;
  }
  if (moderate_memory_fraction_command_line_ < 0) {
    moderate_memory_fraction_ =
        relax ? relaxed_moderate_memory_fraction_ : kModerateMemoryFraction;
  }
  LOG(INFO) << __func__
            << ": critical_memory_fraction_=" << critical_memory_fraction_
            << ", moderate_memory_fraction_=" << moderate_memory_fraction_;
}

void CastSystemMemoryPressureEvaluator::ConfigRelaxMemoryPressureThresholds(
    float relaxed_critical_memory_fraction,
    float relaxed_moderate_memory_fraction) {
  MAKE_SURE_THREAD(ConfigRelaxMemoryPressureThresholds,
                   relaxed_critical_memory_fraction,
                   relaxed_moderate_memory_fraction);

  LOG(INFO) << __func__ << ", " << relaxed_critical_memory_fraction << ", "
            << relaxed_moderate_memory_fraction;

  if (relaxed_critical_memory_fraction > 0) {
    relaxed_critical_memory_fraction_ = relaxed_critical_memory_fraction;
  }
  if (relaxed_moderate_memory_fraction > 0) {
    relaxed_moderate_memory_fraction_ = relaxed_moderate_memory_fraction;
  }
}

void CastSystemMemoryPressureEvaluator::RelaxMemoryPressureThresholds(
    std::string requesting_app_session_id) {
  MAKE_SURE_THREAD(RelaxMemoryPressureThresholds,
                   std::move(requesting_app_session_id));
  apps_needing_relaxed_memory_pressure_thresholds_.insert(
      std::move(requesting_app_session_id));
  AdjustMemoryFractions(true);
}
void CastSystemMemoryPressureEvaluator::RestoreMemoryPressureThresholds(
    const std::string& requesting_app_session_id) {
  MAKE_SURE_THREAD(RestoreMemoryPressureThresholds, requesting_app_session_id);
  apps_needing_relaxed_memory_pressure_thresholds_.erase(
      requesting_app_session_id);
  if (apps_needing_relaxed_memory_pressure_thresholds_.empty()) {
    AdjustMemoryFractions(false);
  }
}

}  // namespace chromecast
