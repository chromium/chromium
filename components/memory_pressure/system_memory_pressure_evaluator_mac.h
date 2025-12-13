// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_dispatch_object.h"
#include "base/byte_count.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace memory_pressure::mac {

class TestSystemMemoryPressureEvaluator;

// Declares the interface for the Mac SystemMemoryPressureEvaluator, which
// reports memory pressure events and status.
class SystemMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator {
 public:
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);
  ~SystemMemoryPressureEvaluator() override;

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

 private:
  friend TestSystemMemoryPressureEvaluator;

  static base::MemoryPressureLevel MemoryPressureLevelForMacMemoryPressureLevel(
      int mac_memory_pressure_level);

  // Returns the raw memory pressure level from the macOS. Exposed for
  // unit testing.
  virtual int GetMacMemoryPressureLevel();

  // Updates |last_pressure_level_| with the current memory pressure level.
  void UpdatePressureLevel();

  // Run |dispatch_callback| on memory pressure notifications from the OS.
  void OnMemoryPressureChanged();

  // Periodically checks the amount of free disk space.
  void CheckDiskSpace();

  // Callback for the disk space check. Updates the pressure level based on the
  // amount of free space.
  void OnDiskSpaceCheckComplete(std::optional<int64_t> free_bytes);

  // Updates the pressure level and manages re-notification timers.
  void UpdatePressureAndManageNotifications();

  // The dispatch source that generates memory pressure change notifications.
  base::apple::ScopedDispatchObject<dispatch_source_t>
      memory_level_event_source_;

  // Timer that will re-notify with the current vote at regular interval.
  base::RepeatingTimer renotify_current_vote_timer_;

  // A task runner that can be used for blocking tasks.
  scoped_refptr<base::SequencedTaskRunner> disk_check_task_runner_;

  // The timer that periodically triggers a disk space check.
  base::RepeatingTimer disk_space_check_timer_;

  // The pressure level calculated from the available disk space.
  base::MemoryPressureLevel disk_pressure_vote_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;

  // The path to the user data directory, used for the disk space check.
  base::FilePath user_data_dir_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMemoryPressureEvaluator> weak_ptr_factory_;
};

}  // namespace memory_pressure::mac

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_
