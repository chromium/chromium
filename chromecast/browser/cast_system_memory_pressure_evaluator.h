// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_system_memory_pressure_evaluator_adjuster.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {

// Memory pressure evaluator for Cast: polls for current memory
// usage periodically and sends memory pressure notifications.
class CastSystemMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator,
      public CastSystemMemoryPressureEvaluatorAdjuster {
 public:
  explicit CastSystemMemoryPressureEvaluator(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);

  CastSystemMemoryPressureEvaluator(const CastSystemMemoryPressureEvaluator&) =
      delete;
  CastSystemMemoryPressureEvaluator& operator=(
      const CastSystemMemoryPressureEvaluator&) = delete;

  ~CastSystemMemoryPressureEvaluator() override;

  // CastSystemMemoryPressureEvaluatorAdjuster implementation:
  void ConfigRelaxMemoryPressureThresholds(
      float relaxed_critical_memory_fraction,
      float relaxed_moderate_memory_fraction) override;
  void RelaxMemoryPressureThresholds(
      std::string requesting_app_session_id) override;
  void RestoreMemoryPressureThresholds(
      const std::string& requesting_app_session_id) override;

 private:
  void PollPressureLevel();
  void UpdateMemoryPressureLevel(
      base::MemoryPressureListener::MemoryPressureLevel new_level);
  void AdjustMemoryFractions(bool relax);

  // Fractions in effect.
  float critical_memory_fraction_;
  float moderate_memory_fraction_;

  // Fractions when the thrsholds are relaxed.
  float relaxed_critical_memory_fraction_;
  float relaxed_moderate_memory_fraction_;

  // When negative, no valid critical/moderate memory fraction present
  // in command line parameters.
  float const critical_memory_fraction_command_line_;
  float const moderate_memory_fraction_command_line_;

  base::flat_set<std::string> apps_needing_relaxed_memory_pressure_thresholds_;

  const int system_reserved_kb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<CastSystemMemoryPressureEvaluator> weak_ptr_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
