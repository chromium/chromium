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

  void OnNetworkAndMainThreadIdle();

  void OnPaintStabilityReached();

  void OnInteractionContentfulPaint();

  void Flush();

 private:
  void RecordTimingMetrics();

  PageStabilityOutcome stability_outcome_ = PageStabilityOutcome::kUnknown;

  bool network_and_main_thread_stability_reached_ = false;
  bool paint_stability_reached_ = false;

  // The time at which it starts to wait for page stabilization.
  base::TimeTicks start_waiting_time_;

  // The time at which it starts to actively monitor page stabilization.
  base::TimeTicks start_monitoring_time_;

  // The time at which the last interaction contentful paint was detected.
  base::TimeTicks last_interaction_contentful_paint_time_;

  base::TimeDelta total_time_between_interaction_contentful_paints_;
  int subsequent_contentful_paint_count_ = 0;

  bool flushed_ = false;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_METRICS_H_
