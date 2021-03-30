// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_MODE_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_MODE_H_

#include "base/component_export.h"

namespace power_scheduler {

// Power modes are "use cases" for the power scheduler. This enum is used both
// as a PowerModeVoter's input to PowerModeArbiter and as the arbiter's output,
// i.e. as the process's current power mode.
enum class PowerMode {
  // Values in ascending priority order. See
  // PowerModeArbiter::ComputeActiveModeLocked.

  // Default mode: none of the other use cases were detected.
  kIdle,

  // The process is playing audio.
  kAudible,

  // A page or tab associated with the process is loading.
  kLoading,

  // A surface rendered by the process is animating.
  kAnimation,

  // The process is responding to user input.
  kResponse,

  // The (Android) app is showing an uninstrumented activity (e.g., Chromium's
  // settings activity) for which we can't determine a more specific use case.
  // Only valid in Chromium's browser process.
  kNonWebActivity,

  // All pages and tabs associated with the process are backgrounded, or the app
  // itself is backgrounded.
  kBackground,

  // The device is connected to an external power source.
  kCharging,

  kMaxValue = kCharging,
};

COMPONENT_EXPORT(POWER_SCHEDULER) const char* PowerModeToString(PowerMode);

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_MODE_H_
