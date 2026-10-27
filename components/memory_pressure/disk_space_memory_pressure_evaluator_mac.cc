// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/disk_space_memory_pressure_evaluator_mac.h"

#include <utility>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace memory_pressure::mac {

namespace {

// The threshold for the critical disk space pressure signal.
constexpr base::ByteSize kCriticalDiskSpaceThreshold = base::MiB(250);

// How often to check for free disk space.
constexpr base::TimeDelta kDiskSpaceCheckPeriod = base::Seconds(5);

}  // namespace

DiskSpaceMemoryPressureEvaluator::DiskSpaceMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
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
}

DiskSpaceMemoryPressureEvaluator::~DiskSpaceMemoryPressureEvaluator() = default;

void DiskSpaceMemoryPressureEvaluator::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!disk_space_check_timer_.IsRunning());
  disk_space_check_timer_.Start(
      FROM_HERE, kDiskSpaceCheckPeriod,
      base::BindRepeating(&DiskSpaceMemoryPressureEvaluator::CheckDiskSpace,
                          weak_ptr_factory_.GetWeakPtr()));
  // Perform an initial check on startup.
  CheckDiskSpace();
}

void DiskSpaceMemoryPressureEvaluator::CheckDiskSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disk_check_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::SysInfo::AmountOfDiskSpace, user_data_dir_),
      base::BindOnce(
          &DiskSpaceMemoryPressureEvaluator::OnDiskSpaceCheckComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

void DiskSpaceMemoryPressureEvaluator::OnDiskSpaceCheckComplete(
    std::optional<base::SysInfo::DiskSpaceInfo> disk_space_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::MemoryPressureLevel new_disk_vote = base::MEMORY_PRESSURE_LEVEL_NONE;

  if (disk_space_info.has_value() &&
      disk_space_info->available < kCriticalDiskSpaceThreshold) {
    new_disk_vote = base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  UpdatePressureAndManageNotifications(new_disk_vote);
}

void DiskSpaceMemoryPressureEvaluator::UpdatePressureAndManageNotifications(
    base::MemoryPressureLevel new_disk_vote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::MemoryPressureLevel old_vote = current_vote();
  if (old_vote == new_disk_vote) {
    return;
  }
  SetCurrentVote(new_disk_vote);
  SendCurrentVote();
}

}  // namespace memory_pressure::mac
