// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_STABILITY_STATE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_STABILITY_STATE_H_

#include <ostream>
#include <string_view>

namespace page_content_annotations {

// Represents the lifecycle states of a single page stability monitoring
// session.
enum class PageStabilityState {
  kInitial,

  // If a delay is specified, wait in this state before starting monitoring.
  kMonitorStartDelay,

  // Before starting the monitor, if a navigation is in-progress, wait for it
  // to commit or fail.
  kWaitForNavigation,

  // Entry point into the state machine. Decides which state to start in.
  kStartMonitoring,

  // The NetworkAndMainThreadStabilityMonitor or PaintStabilityMonitor has
  // determined that the page stability has been reached.
  kMonitorCompleted,

  // Timeout state - this just moves to invoke callback state.
  kTimeout,

  // Delay the callback until the min wait time is reached.
  kDelayCallback,

  // Invoke the callback passed to NotifyWhenStable and cleanup.
  kInvokeCallback,

  // The render frame is about to be deleted (e.g. because of a navigation to
  // a new RenderFrame).
  kRenderFrameGoingAway,

  // The mojo pipeline gets disconnected. This just moves to kDone.
  kMojoDisconnected,

  kDone
};

std::string_view PageStabilityStateToString(PageStabilityState state);

std::ostream& operator<<(std::ostream& o, const PageStabilityState& state);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_STABILITY_STATE_H_
