// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_DELEGATE_H_
#define CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/renderer/actor/journal.h"
#include "components/actor/core/task_id.h"
#include "components/page_content_annotations/core/page_stability_monitor_delegate.h"

namespace actor {

class PageStabilityMetrics;

// Actor-specific implementation of the
// page_content_annotations::PageStabilityMonitorDelegate.
//
// This delegate maintains the integration with the Actor framework by routing
// stability events to the Actor Journal and recording actor-specific timing
// metrics.
class PageStabilityMonitorDelegate
    : public page_content_annotations::PageStabilityMonitorDelegate {
 public:
  // The flag-driven thresholds used for page stability monitoring.
  struct Thresholds {
    // Overall timeout delay before giving up on waiting for page stability.
    base::TimeDelta timeout_delay;
    // Minimum duration to wait before declaring page stability, even if
    // stability is reached earlier.
    base::TimeDelta min_wait;
    // How long the monitor should wait for the initial contentful paint
    // before declaring paint stability.
    base::TimeDelta initial_paint_timeout;
    // How long the monitor should wait for subsequent contentful paints
    // before declaring paint stability.
    base::TimeDelta subsequent_paint_timeout;
  };

  PageStabilityMonitorDelegate(TaskId task_id,
                               Journal& journal,
                               const Thresholds& thresholds);
  ~PageStabilityMonitorDelegate() override;

  // page_content_annotations::PageStabilityMonitorDelegate:
  void WillMoveToState(
      page_content_annotations::PageStabilityState state) override;
  void OnEvent(
      const page_content_annotations::PageStabilityEvent& event) override;
  base::TimeDelta GetTimeoutDelay() const override;
  base::TimeDelta GetMinWait() const override;
  base::TimeDelta GetInitialPaintTimeout() const override;
  base::TimeDelta GetSubsequentPaintTimeout() const override;

 private:
  std::unique_ptr<Journal::PendingAsyncEntry> journal_entry_;
  TaskId task_id_;
  base::raw_ref<Journal> journal_;
  std::unique_ptr<PageStabilityMetrics> metrics_;
  const Thresholds thresholds_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_DELEGATE_H_
