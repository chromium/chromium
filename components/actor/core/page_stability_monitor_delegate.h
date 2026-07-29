// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_PAGE_STABILITY_MONITOR_DELEGATE_H_
#define COMPONENTS_ACTOR_CORE_PAGE_STABILITY_MONITOR_DELEGATE_H_

#include <memory>

#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/page_content_annotations/core/page_stability_monitor_delegate.h"

namespace actor {

class PageStabilityMetrics;

// Actor-specific implementation of the
// page_content_annotations::PageStabilityMonitorDelegate.
//
// This delegate routes page stability events to virtual methods (which
// subclasses can implement to log to the Actor Journal, etc.) and records
// timing metrics.
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

  PageStabilityMonitorDelegate(TaskId task_id, const Thresholds& thresholds);
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

 protected:
  // Called to log a page stability event (kBegin, kEnd, or kInstant).
  virtual void LogEvent(mojom::JournalEntryType type,
                        std::string_view event_name,
                        std::vector<mojom::JournalDetailsPtr> details) = 0;

  TaskId task_id() const { return task_id_; }
  std::optional<std::string> active_state_event_name_;

 private:
  TaskId task_id_;
  std::unique_ptr<PageStabilityMetrics> metrics_;
  const Thresholds thresholds_;
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_PAGE_STABILITY_MONITOR_DELEGATE_H_
