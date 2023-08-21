// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_

#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_dispatch_object.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace memory_pressure {
namespace mac {

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

  static base::MemoryPressureListener::MemoryPressureLevel
  MemoryPressureLevelForMacMemoryPressureLevel(int mac_memory_pressure_level);

  // Returns the raw memory pressure level from the macOS. Exposed for
  // unit testing.
  virtual int GetMacMemoryPressureLevel();

  // Updates |last_pressure_level_| with the current memory pressure level.
  void UpdatePressureLevel();

  // Run |dispatch_callback| on memory pressure notifications from the OS.
  void OnMemoryPressureChanged();

  // The dispatch source that generates memory pressure change notifications.
  base::apple::ScopedDispatchObject<dispatch_source_t>
      memory_level_event_source_;

  // Timer that will re-notify with the current vote at regular interval.
  base::RepeatingTimer renotify_current_vote_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMemoryPressureEvaluator> weak_ptr_factory_;
};

}  // namespace mac
}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_MAC_H_
