// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define CHROMEOS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include <vector>

#include "base/base_export.h"
#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/util/memory_pressure/memory_pressure_voter.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

namespace chromeos {
namespace memory {

////////////////////////////////////////////////////////////////////////////////
// SystemMemoryPressureEvaluator
//
// A class to handle the observation of our free memory. It notifies the
// MemoryPressureListener of memory fill level changes, so that it can take
// action to reduce memory resources accordingly.
class COMPONENT_EXPORT(CHROMEOS_MEMORY) SystemMemoryPressureEvaluator
    : public util::SystemMemoryPressureEvaluator {
 public:
  // The SystemMemoryPressureEvaluator reads the pressure levels from the
  // /sys/kernel/mm/chromeos-low_mem/margin and does not need to be configured.
  //
  // NOTE: You should check that the kernel supports notifications by calling
  // SupportsKernelNotifications() before constructing a new instance of this
  // class.
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<util::MemoryPressureVoter> voter);
  ~SystemMemoryPressureEvaluator() override;

  // GetMarginFileParts returns a vector of the configured margin file values.
  // The margin file contains two or more values, but we're only concerned with
  // the first two. The first represents critical memory pressure, the second
  // is moderate memory pressure level.
  static std::vector<int> GetMarginFileParts();

  // SupportsKernelNotifications will return true if the kernel supports and is
  // configured for notifications on memory availability changes.
  static bool SupportsKernelNotifications();

  // ScheduleEarlyCheck is used by the ChromeOS tab manager delegate to force it
  // to quickly recheck pressure levels after a tab discard or some other
  // action.
  void ScheduleEarlyCheck();

  // Returns the moderate pressure threshold as read from the margin file.
  int ModeratePressureThresholdMBForTesting() const {
    return moderate_pressure_threshold_mb_;
  }

  // Returns the critical pressure threshold as read from the margin file.
  int CriticalPressureThresholdMBForTesting() const {
    return critical_pressure_threshold_mb_;
  }

  // Returns the current system memory pressure evaluator.
  static SystemMemoryPressureEvaluator* Get();

 protected:
  // This constructor is only used for testing.
  SystemMemoryPressureEvaluator(
      const std::string& margin_file,
      bool disable_timer_for_testing,
      std::unique_ptr<util::MemoryPressureVoter> voter);

  static std::vector<int> GetMarginFileParts(const std::string& margin_file);

  void CheckMemoryPressure();

  // Split CheckMemoryPressure and CheckMemoryPressureImpl for testing.
  void CheckMemoryPressureImpl(uint64_t mem_avail_mb);

 private:
  void CheckMemoryPressureAndRecordStatistics();
  int moderate_pressure_threshold_mb_ = 0;
  int critical_pressure_threshold_mb_ = 0;

  // We keep track of how long it has been since we last notified at the
  // moderate level.
  base::TimeTicks last_moderate_notification_;

  // We keep track of how long it's been since we notified on the
  // Memory.PressureLevel metric.
  base::TimeTicks last_pressure_level_report_;

  // A timer to check the memory pressure and to report an UMA metric
  // periodically.
  base::RepeatingTimer checking_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMemoryPressureEvaluator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemMemoryPressureEvaluator);
};

}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
