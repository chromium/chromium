// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"

#include <windows.h>
#include <winternl.h>

#include <psapi.h>

#include <algorithm>
#include <memory>

#include "base/byte_count.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_types.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace memory_pressure {
namespace win {

namespace {

// When enabled, allows setting custom thresholds for commit-based
// memory pressure detection via the |kCommitAvailableCriticalThresholdMB|
// and |kCommitAvailableModerateThresholdMB| parameters.
BASE_FEATURE(kCommitAvailableMemoryPressureThresholds,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default thresholds for commit-based memory pressure detection.
constexpr base::ByteCount kDefaultCommitAvailableCriticalThreshold =
    base::MiB(200);
constexpr base::ByteCount kDefaultCommitAvailableModerateThreshold =
    base::MiB(500);

// The amount of commit available below which the system is considered to be
// under critical memory pressure. The default value is equal to
// kSmallMemoryDefaultCriticalThreshold (200MiB).
BASE_FEATURE_PARAM(int,
                   kCommitAvailableCriticalThresholdMB,
                   &kCommitAvailableMemoryPressureThresholds,
                   "CommitAvailableCriticalThresholdMB",
                   kDefaultCommitAvailableCriticalThreshold.InMiB());

// The amount of commit available (in MB) below which the system is considered
// to be under moderate memory pressure. The default value is equal to
// kSmallMemoryDefaultModerateThresholdMb (500).
BASE_FEATURE_PARAM(int,
                   kCommitAvailableModerateThresholdMB,
                   &kCommitAvailableMemoryPressureThresholds,
                   "CommitAvailableModerateThresholdMB",
                   kDefaultCommitAvailableModerateThreshold.InMiB());

// Controls the frequency at which memory pressure is evaluated on Windows.
BASE_FEATURE(kWindowsMemoryPressurePeriod,
             "WinMemoryPressurePeriod",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kWinMemoryPressurePeriodParam,
                   &kWindowsMemoryPressurePeriod,
                   "period",
                   SystemMemoryPressureEvaluator::kDefaultPeriod);

// Constant for early exit commit threshold. Used for the initial pressure check
// to avoid activating the feature study group for users with ample memory.
// Value based on Memory.CommitAvailableMB UMA, aiming to capture a population
// similar in size (~13%) to the existing physical memory signal.
constexpr base::ByteCount kEarlyExitCommitThreshold = base::GiB(2);

// Implements ObjectWatcher::Delegate by forwarding to a provided callback.
class MemoryPressureWatcherDelegate
    : public base::win::ObjectWatcher::Delegate {
 public:
  MemoryPressureWatcherDelegate(base::win::ScopedHandle handle,
                                base::OnceClosure callback);
  ~MemoryPressureWatcherDelegate() override;
  MemoryPressureWatcherDelegate(const MemoryPressureWatcherDelegate& other) =
      delete;
  MemoryPressureWatcherDelegate& operator=(
      const MemoryPressureWatcherDelegate&) = delete;

  void ReplaceWatchedHandleForTesting(base::win::ScopedHandle handle);
  void SetCallbackForTesting(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  void OnObjectSignaled(HANDLE handle) override;

  base::win::ScopedHandle handle_;
  base::win::ObjectWatcher watcher_;
  base::OnceClosure callback_;
};

MemoryPressureWatcherDelegate::MemoryPressureWatcherDelegate(
    base::win::ScopedHandle handle,
    base::OnceClosure callback)
    : handle_(std::move(handle)), callback_(std::move(callback)) {
  DCHECK(handle_.is_valid());
  CHECK(watcher_.StartWatchingOnce(handle_.Get(), this));
}

MemoryPressureWatcherDelegate::~MemoryPressureWatcherDelegate() = default;

void MemoryPressureWatcherDelegate::ReplaceWatchedHandleForTesting(
    base::win::ScopedHandle handle) {
  if (watcher_.IsWatching()) {
    watcher_.StopWatching();
  }
  handle_ = std::move(handle);
  CHECK(watcher_.StartWatchingOnce(handle_.Get(), this));
}

void MemoryPressureWatcherDelegate::OnObjectSignaled(HANDLE handle) {
  DCHECK_EQ(handle, handle_.Get());
  std::move(callback_).Run();
}

}  // namespace

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : SystemMemoryPressureEvaluator(kPhysicalMemoryDefaultModerateThreshold,
                                    kPhysicalMemoryDefaultCriticalThreshold,
                                    std::move(voter)) {}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    base::ByteCount moderate_threshold,
    base::ByteCount critical_threshold,
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_threshold_(moderate_threshold),
      critical_threshold_(critical_threshold),
      moderate_pressure_repeat_count_(0) {
  DCHECK_GE(moderate_threshold_, critical_threshold_);
  DCHECK(critical_threshold_.is_positive());
  StartObserving();
}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  StopObserving();
}


void SystemMemoryPressureEvaluator::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // base::Unretained is safe in this case because this class owns the
  // timer, and will cancel the timer on destruction.
  timer_.Start(
      FROM_HERE, kWinMemoryPressurePeriodParam.Get(),
      base::BindRepeating(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                          base::Unretained(this)));
}

void SystemMemoryPressureEvaluator::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
}

void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the previous pressure level and update the current one.
  base::MemoryPressureLevel old_vote = current_vote();
  SetCurrentVote(CalculateCurrentPressureLevel());

  // |notify| will be set to true if MemoryPressureListeners need to be
  // notified of a memory pressure level state change.

  bool notify = false;
  switch (current_vote()) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
      // Always notify if the state changed.
      if (old_vote != current_vote()) {
        notify = true;
      }
      break;

    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
      if (old_vote != current_vote()) {
        // This is a new transition to moderate pressure so notify.
        moderate_pressure_repeat_count_ = 0;
        notify = true;
      } else {
        // Already in moderate pressure, only notify if sustained over the
        // cooldown period.
        const int kModeratePressureCooldownCycles =
            kModeratePressureCooldown / kWinMemoryPressurePeriodParam.Get();
        if (++moderate_pressure_repeat_count_ ==
            kModeratePressureCooldownCycles) {
          moderate_pressure_repeat_count_ = 0;
          notify = true;
        }
      }
      break;

    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Always notify of critical pressure levels.
      notify = true;
      break;
  }

  SendCurrentVote(notify);
}

base::MemoryPressureLevel
SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel() {
  MEMORYSTATUSEX mem_status = {};
  bool got_system_memory_status = GetSystemMemoryStatus(mem_status);

  if (!got_system_memory_status) {
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }
  RecordCommitHistograms(mem_status);

  // How much physical system memory is available for use right now.
  base::ByteCount phys_free =
      base::ByteCount::FromUnsigned(mem_status.ullAvailPhys);

  // The maximum amount of memory the current process can commit.
  base::ByteCount commit_available =
      base::ByteCount::FromUnsigned(mem_status.ullAvailPageFile);

  if (phys_free > moderate_threshold_ &&
      commit_available > kEarlyExitCommitThreshold) {
    // No memory pressure under any of the 2 detection systems. Return
    // early to avoid activating the experiment for clients who don't
    // have memory pressure.
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  if (base::FeatureList::IsEnabled(kCommitAvailableMemoryPressureThresholds)) {
    if (commit_available <
        base::MiB(kCommitAvailableCriticalThresholdMB.Get())) {
      return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
    }

    if (commit_available <
        base::MiB(kCommitAvailableModerateThresholdMB.Get())) {
      return base::MEMORY_PRESSURE_LEVEL_MODERATE;
    }

    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  // TODO(chrisha): This should eventually care about address space pressure,
  // but the browser process (where this is running) effectively never runs out
  // of address space. Renderers occasionally do, but it does them no good to
  // have the browser process monitor address space pressure. Long term,
  // renderers should run their own address space pressure monitors and act
  // accordingly, with the browser making cross-process decisions based on
  // system memory pressure.

  // Determine if the physical memory is under critical memory pressure.
  if (phys_free <= critical_threshold_) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  // Determine if the physical memory is under moderate memory pressure.
  if (phys_free <= moderate_threshold_) {
    return base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  // No memory pressure was detected.
  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

bool SystemMemoryPressureEvaluator::GetSystemMemoryStatus(
    MEMORYSTATUSEX& mem_status) {
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status)) {
    return false;
  }
  return true;
}

void SystemMemoryPressureEvaluator::RecordCommitHistograms(
    const MEMORYSTATUSEX& mem_status) {
  // Calculate commit limit.
  base::ByteCount commit_limit =
      base::ByteCount::FromUnsigned(mem_status.ullTotalPageFile);

  // Calculate amount of available commit space.
  base::ByteCount commit_available =
      base::ByteCount::FromUnsigned(mem_status.ullAvailPageFile);

  base::UmaHistogramCounts10M("Memory.CommitLimitMB",
                              base::saturated_cast<int>(commit_limit.InMiB()));
  base::UmaHistogramCounts10M(
      "Memory.CommitAvailableMB",
      base::saturated_cast<int>(commit_available.InMiB()));

  // Calculate percentage used
  int percentage_used;
  if (commit_limit.is_zero()) {
    // Handle division by zero.
    percentage_used = 0;
  } else {
    uint64_t percentage_remaining =
        commit_available.InBytesF() * 100 / commit_limit.InBytesF();
    percentage_used = static_cast<int>(
        percentage_remaining > 100 ? 0u : 100 - percentage_remaining);
  }

  base::UmaHistogramPercentage("Memory.CommitPercentageUsed", percentage_used);
}

}  // namespace win
}  // namespace memory_pressure
