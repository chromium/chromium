// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_DELEGATE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_DELEGATE_H_

#include "components/page_content_annotations/core/page_stability_event.h"
#include "components/page_content_annotations/core/page_stability_state.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace page_content_annotations {

// Delegate interface for feature-specific hooks within the
// PageStabilityMonitor.
//
// This allows the monitor to remain agnostic of application-specific concepts
// while still providing a way for features to observe and log stability events.
class PageStabilityMonitorDelegate {
 public:
  virtual ~PageStabilityMonitorDelegate() = default;

  // Called just before the monitor transitions to a new state.
  virtual void WillMoveToState(PageStabilityState state) {}

  // Called when a discrete stability-related event occurs.
  virtual void OnEvent(const PageStabilityEvent& event) {}

  // Configuration overrides for timeouts and wait periods.
  virtual base::TimeDelta GetTimeoutDelay() const;
  virtual base::TimeDelta GetMinWait() const;
  virtual base::TimeDelta GetInitialPaintTimeout() const;
  virtual base::TimeDelta GetSubsequentPaintTimeout() const;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_DELEGATE_H_
