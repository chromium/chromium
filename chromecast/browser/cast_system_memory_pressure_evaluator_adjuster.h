// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_ADJUSTER_H_
#define CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_ADJUSTER_H_

#include <string>

namespace chromecast {
class CastSystemMemoryPressureEvaluatorAdjuster {
 public:
  CastSystemMemoryPressureEvaluatorAdjuster() = default;
  virtual ~CastSystemMemoryPressureEvaluatorAdjuster() = default;

  // The three functions below can be called from any thread.
  // Negative fractions are invalid.
  virtual void ConfigRelaxMemoryPressureThresholds(
      float relaxed_critical_memory_fraction,
      float relaxed_moderate_memory_fraction);
  virtual void RelaxMemoryPressureThresholds(
      std::string requesting_app_session_id);
  virtual void RestoreMemoryPressureThresholds(
      const std::string& requesting_app_session_id);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_ADJUSTER_H_