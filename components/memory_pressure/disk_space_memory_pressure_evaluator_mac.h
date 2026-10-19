// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_DISK_SPACE_MEMORY_PRESSURE_EVALUATOR_MAC_H_
#define COMPONENTS_MEMORY_PRESSURE_DISK_SPACE_MEMORY_PRESSURE_EVALUATOR_MAC_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace base {
class SequencedTaskRunner;
}

namespace memory_pressure::mac {

// Declares the interface for the Mac DiskSpaceMemoryPressureEvaluator, which
// periodically checks available disk space and reports memory pressure events
// and status if disk space falls below a critical threshold.
class DiskSpaceMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator {
 public:
  explicit DiskSpaceMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);
  ~DiskSpaceMemoryPressureEvaluator() override;

  DiskSpaceMemoryPressureEvaluator(const DiskSpaceMemoryPressureEvaluator&) =
      delete;
  DiskSpaceMemoryPressureEvaluator& operator=(
      const DiskSpaceMemoryPressureEvaluator&) = delete;

  // Performs an initial disk space check and starts the periodic checks. Kept
  // separate from the constructor so tests can drive
  // OnDiskSpaceCheckComplete() directly without touching the file system.
  void Start();

 protected:
  // Callback for the disk space check. Updates the pressure level based on the
  // amount of free space.
  void OnDiskSpaceCheckComplete(
      std::optional<base::SysInfo::DiskSpaceInfo> disk_space_info);

 private:
  // Posts a blocking query for the amount of free disk space.
  void CheckDiskSpace();

  // Updates the pressure level and sends the current vote if it changed.
  void UpdatePressureAndManageNotifications(
      base::MemoryPressureLevel new_disk_vote);

  // A task runner that can be used for blocking tasks.
  scoped_refptr<base::SequencedTaskRunner> disk_check_task_runner_;

  // The timer that periodically triggers a disk space check.
  base::RepeatingTimer disk_space_check_timer_;

  // The path to the user data directory, used for the disk space check.
  base::FilePath user_data_dir_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DiskSpaceMemoryPressureEvaluator> weak_ptr_factory_;
};

}  // namespace memory_pressure::mac

#endif  // COMPONENTS_MEMORY_PRESSURE_DISK_SPACE_MEMORY_PRESSURE_EVALUATOR_MAC_H_
