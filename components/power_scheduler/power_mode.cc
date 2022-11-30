// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode.h"

#include "base/trace_event/trace_event.h"

namespace power_scheduler {

const char* PowerModeToString(PowerMode mode) {
  switch (mode) {
    case PowerMode::kIdle:
      return "Idle";
    case PowerMode::kNopAnimation:
      return "NopAnimation";
    case PowerMode::kSmallMainThreadAnimation:
      return "SmallMainThreadAnimation";
    case PowerMode::kSmallAnimation:
      return "SmallAnimation";
    case PowerMode::kMediumMainThreadAnimation:
      return "MediumMainThreadAnimation";
    case PowerMode::kMediumAnimation:
      return "MediumAnimation";
    case PowerMode::kAudible:
      return "Audible";
    case PowerMode::kVideoPlayback:
      return "VideoPlayback";
    case PowerMode::kMainThreadAnimation:
      return "MainThreadAnimation";
    case PowerMode::kScriptExecution:
      return "ScriptExecution";
    case PowerMode::kLoading:
      return "Loading";
    case PowerMode::kAnimation:
      return "Animation";
    case PowerMode::kLoadingAnimation:
      return "LoadingAnimation";
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
