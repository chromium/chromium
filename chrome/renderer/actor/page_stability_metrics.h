// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_
#define CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_

#include "base/time/time.h"
#include "chrome/common/actor/page_stability_metrics_common.h"
#include "chrome/renderer/actor/page_stability_monitor.h"

namespace actor {

class PageStabilityMetrics {
 public:
  PageStabilityMetrics();
  ~PageStabilityMetrics();

  void Start();

  void WillMoveToState(PageStabilityMonitor::State state);

 private:
  void RecordTimingMetrics();

  PageStabilityOutcome stability_outcome_ = PageStabilityOutcome::kUnknown;

  // The time at which it starts to wait for page stabilization.
  base::TimeTicks start_waiting_time_;

  // The time at which it starts to actively monitor page stabilization.
  base::TimeTicks start_monitoring_time_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_
