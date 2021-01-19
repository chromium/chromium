// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode.h"

#include "base/trace_event/trace_event.h"

namespace power_scheduler {

const char* PowerModeToString(PowerMode mode) {
  switch (mode) {
    case PowerMode::kIdle:
      return "Idle";
    case PowerMode::kAudible:
      return "Audible";
    case PowerMode::kLoading:
      return "Loading";
    case PowerMode::kAnimation:
      return "Animation";
    case PowerMode::kResponse:
      return "Response";
    case PowerMode::kNonWebActivity:
      return "NonWebActivity";
    case PowerMode::kBackground:
      return "Background";
    case PowerMode::kCharging:
      return "Charging";
  }
}

}  // namespace power_scheduler
