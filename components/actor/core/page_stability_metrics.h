// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_PAGE_STABILITY_METRICS_H_
#define COMPONENTS_ACTOR_CORE_PAGE_STABILITY_METRICS_H_

#include "components/page_content_annotations/core/page_stability_state.h"

namespace actor {

// Abstract interface for recording metrics related to page stability
// monitoring.
class PageStabilityMetrics {
 public:
  PageStabilityMetrics() = default;
  PageStabilityMetrics(const PageStabilityMetrics&) = delete;
  PageStabilityMetrics& operator=(const PageStabilityMetrics&) = delete;
  virtual ~PageStabilityMetrics() = default;

  // Called when page stability waiting starts.
  virtual void Start() = 0;

  // Called when moving to a new `PageStabilityState`.
  virtual void WillMoveToState(
      page_content_annotations::PageStabilityState state) = 0;

  // Called when the network and main-thread idle condition is reached.
  virtual void OnNetworkAndMainThreadIdle() {}

  // Called when paint stability is reached.
  virtual void OnPaintStabilityReached() {}

  // Called when an interaction contentful paint occurs.
  virtual void OnInteractionContentfulPaint() {}

  // Flushes any remaining unrecorded metrics.
  virtual void Flush() {}
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_PAGE_STABILITY_METRICS_H_
