// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_WITH_PRIORITY_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_WITH_PRIORITY_H_

#include <stddef.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/infobars/core/infobar_container.h"
#include "components/infobars/core/infobar_delegate.h"

namespace infobars {

// This is a specialized `InfoBarContainer` that manages the visibility of
// infobars based on a priority system. Unlike the base class that displays all
// infobars as they are added, this implementation uses an admission control and
// queuing mechanism to ensure that high-priority infobars are surfaced to the
// user, while less critical ones are deferred.
//
// When an infobar is added, its priority (`CRITICAL`, `DEFAULT`, or `LOW`) is
// evaluated. It is only shown if it is not blocked by a higher-priority
// infobar that is already visible or waiting in the pending queue. If blocked,
// the infobar is enqueued. The container continuously attempts to "promote"
// infobars from the queue to a visible state as soon as higher-priority
// infobars are removed. This ensures the most important notifications are
// always given precedence. The class also handles the prevention of duplicate
// infobars in the pending queue.
class InfoBarContainerWithPriority : public InfoBarContainer {
 public:
  explicit InfoBarContainerWithPriority(Delegate* delegate);
  ~InfoBarContainerWithPriority() override;

  InfoBarContainerWithPriority(const InfoBarContainerWithPriority&) = delete;
  const InfoBarContainerWithPriority& operator=(
      const InfoBarContainerWithPriority&) = delete;

  // InfoBarContainer overrides:
  void ChangeInfoBarManager(InfoBarManager* infobar_manager) override;

 protected:
  // InfoBarContainer overrides:
  void OnInfoBarAdded(InfoBar* infobar) override;
  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(InfoBar* old_infobar, InfoBar* new_infobar) override;

 private:
  // Represents the description of an infobar entry in the queue.
  struct PendingInfoBarEntry {
    // Non-owned infobar instance that is still waiting to be surfaced.
    raw_ptr<InfoBar> infobar;

    // Priority of this infobar in the queue (critical > default > low).
    InfoBarDelegate::InfobarPriority priority;

    // Timestamp indicating when this infobar entry was enqueued. Primarily used
    // for metrics and debugging.
    base::TimeTicks enqueued_at;
  };

  // Represents the description of an infobar currently surfacing.
  struct VisibleEntry {
    raw_ptr<InfoBar> infobar;
    InfoBarDelegate::InfobarPriority priority;
  };

  // Decides whether the infobar can be shown right now
  // (visible slots available, no higher-priority items blocking) or must be
  // queued for later promotion.
  void AdmitOrQueue(InfoBar* infobar,
                    InfoBarDelegate::InfobarPriority priority);

  // Enqueues an infobar in a deterministic position: first by priority
  // (descending), then by sequence (ascending). This guarantees a stable
  // ordering across enqueue operations.
  void EnqueueInfoBar(InfoBar* infobar,
                      InfoBarDelegate::InfobarPriority priority);

  // Tries to surface queued infobars in priority order:
  // 1) fill CRITICAL up to cap,
  // 2) fill DEFAULT if no CRITICAL visible/queued,
  // 3) fill LOW if no DEFAULT visible/queued.
  void Promote();
  void PromoteInfobarsOfPriority(InfoBarDelegate::InfobarPriority priority,
                                 size_t priority_cap);

  // Extends base AddInfoBar with priority tracking.
  void AddInfoBarAndTrack(InfoBar* infobar,
                          size_t position,
                          bool animate,
                          InfoBarDelegate::InfobarPriority priority);

  // Checks whether the given infobar is already present in the pending queue
  // (based on delegate equality). Used to avoid enqueuing logical duplicates.
  bool IsDuplicateOfPending(InfoBar* infobar) const;

  // Marks an infobar as visible in the appropriate priority. Must be kept in
  // sync with the actual AddInfoBar() call that made it visible.
  void MarkVisible(InfoBar* infobar, InfoBarDelegate::InfobarPriority priority);

  // Removes an infobar from whatever visible priority it belongs to (critical,
  // default, or low). Safe to call even if the infobar was not tracked.
  // Returns the number of elements removed.
  size_t ClearVisible(InfoBar* infobar);

  // Return the number of infobar currently visible for the given priority.
  size_t CountVisible(InfoBarDelegate::InfobarPriority priority) const;

  // Returns true if the `pending` queue already contains at least one infobar
  // with the given `priority`. Used to block lower priorities while higher ones
  // are either visible or waiting.
  bool HasPendingOfPriority(InfoBarDelegate::InfobarPriority priority) const;

  // Helper used to record the infobar pending queue size.
  void RecordInfoBarPendingQueueSize();

  // List of infobars that could not be shown immediately because their
  // priority was blocked (e.g. DEFAULT while a CRITICAL is visible). Ordered
  // deterministically by (priority desc, seq asc).
  std::vector<PendingInfoBarEntry> pending_infobars_;

  // This represent the list of infobars currently visible.
  std::vector<VisibleEntry> visible_;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_WITH_PRIORITY_H_
