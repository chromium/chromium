// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stddef.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace memory_pressure::mac {

namespace {
// When enabled, the moderate memory pressure signals on macOS are ignored and
// treated as 'none'. This is to experiment with the idea that the 'warn'
// level signal from the OS is not always an accurate or useful signal.
BASE_FEATURE(kSkipModerateMemoryPressureLevelMac,
             "SkipModerateMemoryPressureLevelMac",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature controls the critical memory pressure signal based on low disk
// space. Disabling this feature turns off the disk space check entirely.
BASE_FEATURE(kMacCriticalDiskSpacePressure,
             "MacCriticalDiskSpacePressure",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default threshold in megabytes for the critical disk space pressure
// signal.
constexpr int kDefaultCriticalDiskSpaceMb = 250;
const int64_t kBytesPerMb = 1024 * 1024;

// Defines the threshold in megabytes for the critical disk space pressure
// signal. This is a parameter for the kMacCriticalDiskSpacePressure feature.
BASE_FEATURE_PARAM(int,
                   kMacCriticalDiskSpacePressureThresholdMB,
                   &kMacCriticalDiskSpacePressure,
                   "MacCriticalDiskSpacePressureThresholdMB",
                   kDefaultCriticalDiskSpaceMb);

// How often to check for free disk space.
constexpr base::TimeDelta kDiskSpaceCheckPeriod = base::Seconds(5);
}  // namespace

base::MemoryPressureListener::MemoryPressureLevel
SystemMemoryPressureEvaluator::MemoryPressureLevelForMacMemoryPressureLevel(
    int mac_memory_pressure_level) {
  switch (mac_memory_pressure_level) {
    case DISPATCH_MEMORYPRESSURE_NORMAL:
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
    case DISPATCH_MEMORYPRESSURE_WARN:
      if (base::FeatureList::IsEnabled(kSkipModerateMemoryPressureLevelMac)) {
        return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
      }
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
    case DISPATCH_MEMORYPRESSURE_CRITICAL:
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      memory_level_event_source_(dispatch_source_create(
          DISPATCH_SOURCE_TYPE_MEMORYPRESSURE,
          0,
          DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL |
              DISPATCH_MEMORYPRESSURE_NORMAL,
          dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0))),
      renotify_current_vote_timer_(
          FROM_HERE,
          kRenotifyVotePeriod,
          base::BindRepeating(&SystemMemoryPressureEvaluator::SendCurrentVote,
                              base::Unretained(this),
                              /*notify=*/true)),
      disk_check_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      weak_ptr_factory_(this) {
  // A check for available disk space is necessary to generate a
  // low-disk-space pressure signal.
  //
  // To ensure the correct disk volume is checked, this implementation uses
  // the user's home directory path, retrieved via `base::PathService`. On
  // macOS, the browser's data directory is a subdirectory of home, so this
  // correctly targets the volume most relevant to browser performance.
  base::PathService::Get(base::DIR_HOME, &user_data_dir_);

  // WeakPtr needed because there is no guarantee that |this| is still be alive
  // when the task posted to the TaskRunner or event handler runs.
  base::WeakPtr<SystemMemoryPressureEvaluator> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  scoped_refptr<base::TaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  // Attach an event handler to the memory pressure event source.
  if (memory_level_event_source_.get()) {
    dispatch_source_set_event_handler(memory_level_event_source_.get(), ^{
      task_runner->PostTask(
          FROM_HERE,
          base::BindRepeating(
              &SystemMemoryPressureEvaluator::OnMemoryPressureChanged,
              weak_this));
    });

    // Start monitoring the event source.
    dispatch_resume(memory_level_event_source_.get());
  }

  if (base::FeatureList::IsEnabled(kMacCriticalDiskSpacePressure)) {
    disk_space_check_timer_.Start(
        FROM_HERE, kDiskSpaceCheckPeriod,
        base::BindRepeating(&SystemMemoryPressureEvaluator::CheckDiskSpace,
                            weak_this));
    // Perform an initial check on startup.
    CheckDiskSpace();
  }
}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  // Remove the memory pressure event source.
  if (memory_level_event_source_.get()) {
    dispatch_source_cancel(memory_level_event_source_.get());
  }
}

int SystemMemoryPressureEvaluator::GetMacMemoryPressureLevel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the raw memory pressure level from macOS.
  int mac_memory_pressure_level;
  size_t length = sizeof(int);
  sysctlbyname("kern.memorystatus_vm_pressure_level",
               &mac_memory_pressure_level, &length, nullptr, 0);

  return mac_memory_pressure_level;
}

void SystemMemoryPressureEvaluator::UpdatePressureLevel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the current macOS pressure level and convert to the corresponding
  // Chrome pressure level.
  auto os_pressure_level =
      MemoryPressureLevelForMacMemoryPressureLevel(GetMacMemoryPressureLevel());

  // The effective pressure level is the most severe of the OS-reported level
  // and our disk-space-derived level. If the disk pressure feature is disabled,
  // `disk_pressure_vote_` will always be `NONE`.
  auto effective_pressure_level =
      std::max(os_pressure_level, disk_pressure_vote_);

  SetCurrentVote(effective_pressure_level);
}

void SystemMemoryPressureEvaluator::OnMemoryPressureChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdatePressureAndManageNotifications();
}

void SystemMemoryPressureEvaluator::CheckDiskSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disk_check_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, user_data_dir_),
      base::BindOnce(&SystemMemoryPressureEvaluator::OnDiskSpaceCheckComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemMemoryPressureEvaluator::OnDiskSpaceCheckComplete(
    int64_t free_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::MemoryPressureListener::MemoryPressureLevel new_disk_vote =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  const int64_t threshold_mb = kMacCriticalDiskSpacePressureThresholdMB.Get();
  // The minimum free disk space in MB before dispatching a critical memory
  // pressure signal.
  const int64_t critical_disk_space_bytes = threshold_mb * kBytesPerMb;

  if (free_bytes != -1 && free_bytes < critical_disk_space_bytes) {
    new_disk_vote =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  if (disk_pressure_vote_ != new_disk_vote) {
    disk_pressure_vote_ = new_disk_vote;
    UpdatePressureAndManageNotifications();
  }
}

void SystemMemoryPressureEvaluator::UpdatePressureAndManageNotifications() {
  // The OS has sent a notification that the memory pressure level has changed.
  // Go through the normal memory pressure level checking mechanism so that
  // |current_vote_| and UMA get updated to the current value.
  UpdatePressureLevel();

  // Run the callback that's waiting on memory pressure change notifications.
  // The convention is to not send notifiations on memory pressure returning to
  // normal.
  bool notify = current_vote() !=
                base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  SendCurrentVote(notify);

  if (notify) {
    renotify_current_vote_timer_.Reset();
  } else {
    renotify_current_vote_timer_.Stop();
  }
}

}  // namespace memory_pressure::mac
