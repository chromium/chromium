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
//
// Infobars the user cannot close bypass the queue and do not count toward any
// cap; otherwise they would block everything queued behind them indefinitely.
class InfoBarContainerWithPriority : public InfoBarContainer {
 public:
  explicit InfoBarContainerWithPriority(Delegate* delegate);
  ~InfoBarContainerWithPriority() override;

  InfoBarContainerWithPriority(const InfoBarContainerWithPriority&) = delete;
  const InfoBarContainerWithPriority& operator=(
      const InfoBarContainerWithPriority&) = delete;

  // InfoBarContainer overrides:
  void ChangeInfoBarManager(InfoBarManager* infobar_manager) override;

  bool HasPendingInfoBars() const { return !pending_infobars_.empty(); }

  size_t PendingInfoBarCount() const { return pending_infobars_.size(); }

 protected:
  // InfoBarContainer overrides:
  void OnInfoBarAdded(InfoBar* infobar) override;
  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(InfoBar* old_infobar, InfoBar* new_infobar) override;

  virtual size_t GetInfoBarPriorityCapFor(
      InfoBarDelegate::InfobarPriority priority) const;

  // Invoked whenever the size of the pending queue changes, with the new
  // number of queued infobars. Platforms can override this to surface the
  // queue to the user, so the prioritization "virtual stack" is visually
  // noticeable instead of silent.
  virtual void PlatformSpecificPendingInfoBarCountChanged(size_t count) {}

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
    bool closeable = true;
  };

  // Returns true if `infobar` can be shown right now rather than queued.
  bool CanAdmit(const InfoBar* infobar,
                InfoBarDelegate::InfobarPriority priority) const;

  // Decides whether the infobar can be shown right now
  // (visible slots available, no higher-priority items blocking) or must be
  // queued for later promotion. Unless `allow_duplicate` is true, an infobar
  // whose delegate equals one already queued is dropped without being
  // tracked. The replace path must pass true because
  // InfoBarManager::ReplaceInfoBar, unlike AddInfoBar, performs no
  // EqualsDelegate dedup, and an untracked manager-owned infobar crashes on
  // its eventual removal.
  void AdmitOrQueue(InfoBar* infobar,
                    InfoBarDelegate::InfobarPriority priority,
                    bool animate,
                    bool allow_duplicate = false);

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

  // Returns the number of closeable infobars visible for the given priority.
  size_t CountVisible(InfoBarDelegate::InfobarPriority priority) const;

  // Returns true if the `pending` queue already contains at least one infobar
  // with the given `priority`. Used to block lower priorities while higher ones
  // are either visible or waiting.
  bool HasPendingOfPriority(InfoBarDelegate::InfobarPriority priority) const;

  // Helper used to record the infobar pending queue size.
  void RecordInfoBarPendingQueueSize();

  // Calls PlatformSpecificPendingInfoBarCountChanged() if the pending queue
  // size changed since the last notification. Must be invoked at the end of
  // every operation that may mutate `pending_infobars_`.
  void NotifyPendingCountChangedIfNeeded();

  // List of infobars that could not be shown immediately because their
  // priority was blocked (e.g. DEFAULT while a CRITICAL is visible). Ordered
  // deterministically by (priority desc, seq asc).
  std::vector<PendingInfoBarEntry> pending_infobars_;

  // This represent the list of infobars currently visible.
  std::vector<VisibleEntry> visible_;

  // The pending queue size that was last reported through
  // PlatformSpecificPendingInfoBarCountChanged(), used to avoid redundant
  // notifications.
  size_t last_notified_pending_count_ = 0;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_WITH_PRIORITY_H_
