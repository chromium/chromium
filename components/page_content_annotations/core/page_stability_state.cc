// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_stability_state.h"

#include <iostream>
#include <string_view>

#include "base/notreached.h"

namespace page_content_annotations {

std::string_view PageStabilityStateToString(PageStabilityState state) {
  switch (state) {
    case PageStabilityState::kInitial:
      return "Initial";
    case PageStabilityState::kMonitorStartDelay:
      return "MonitorStartDelay";
    case PageStabilityState::kWaitForNavigation:
      return "WaitForNavigation";
    case PageStabilityState::kStartMonitoring:
      return "StartMonitoring";
    case PageStabilityState::kMonitorCompleted:
      return "MonitorCompleted";
    case PageStabilityState::kTimeout:
      return "Timeout";
    case PageStabilityState::kDelayCallback:
      return "DelayCallback";
    case PageStabilityState::kInvokeCallback:
      return "InvokeCallback";
    case PageStabilityState::kRenderFrameGoingAway:
      return "RenderFrameGoingAway";
    case PageStabilityState::kMojoDisconnected:
      return "MojoDisconnected";
    case PageStabilityState::kDone:
      return "Done";
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& o, const PageStabilityState& state) {
  return o << PageStabilityStateToString(state);
}

}  // namespace page_content_annotations
