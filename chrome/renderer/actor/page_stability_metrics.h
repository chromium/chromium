// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_
#define CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_

#include "chrome/common/actor/page_stability_metrics_common.h"
#include "chrome/renderer/actor/page_stability_monitor.h"

namespace actor {

class PageStabilityMetrics {
 public:
  PageStabilityMetrics();
  ~PageStabilityMetrics();

  void WillMoveToState(PageStabilityMonitor::State state);

 private:
  PageStabilityOutcome stability_outcome_ = PageStabilityOutcome::kUnknown;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_
