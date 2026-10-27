// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_

#include <dispatch/dispatch.h>

#include <memory>

#include "base/apple/scoped_dispatch_object.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/memory_pressure/memory_pressure_level_reporter.h"
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

  // Updates |os_pressure_level_| with the current memory pressure level.
  void UpdatePressureLevel();

  // Run |dispatch_callback| on memory pressure notifications from the OS.
  void OnMemoryPressureChanged();

  // Updates the pressure level and sends the current vote if it changed.
  void UpdatePressureAndManageNotifications();

  // The dispatch source that generates memory pressure change notifications.
  base::apple::ScopedDispatchObject<dispatch_source_t>
      memory_level_event_source_;

  // The last computed OS-reported memory pressure level.
  base::MemoryPressureLevel os_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;

  SEQUENCE_CHECKER(sequence_checker_);

  base::MemoryPressureLevel last_os_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;
  std::unique_ptr<MemoryPressureLevelReporter> os_reporter_;

  base::WeakPtrFactory<SystemMemoryPressureEvaluator> weak_ptr_factory_;
};

}  // namespace memory_pressure::mac

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_
