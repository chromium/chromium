// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_BATCH_PROGRESS_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_BATCH_PROGRESS_TRACKER_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "components/infobars/core/infobar_manager.h"

namespace infobars {
class InfoBar;
}  // namespace infobars

// Tracks the sequential ("1 of 3" -> "2 of 3" -> "3 of 3") batch progress for
// a single tab's `InfoBarManager`.
//
// Owned by `InfoBarManager` via `base::SupportsUserData` so batch progress is
// scoped to the tab rather than to a specific browser window's
// `InfoBarContainerView` (surviving tab switches, split-view pane changes,
// background-tab dismissals, and moving a tab into another window).
//
// Removals and replacements are driven solely via `InfoBarManager::Observer`.
// Call `GetOrCreate()` before subscribing a container observer to the same
// `InfoBarManager` so this tracker reconciles lifecycle changes prior to the
// container querying `ResolvePosition()`.
class BatchProgressTracker : public base::SupportsUserData::Data,
                             public infobars::InfoBarManager::Observer {
 public:
  struct Position {
    size_t index = 0;
    size_t total = 0;

    constexpr bool should_show() const {
      return index > 0 && total > 1 && index <= total;
    }
    friend constexpr bool operator==(const Position&,
                                     const Position&) = default;
  };

  // Returns true if `infobar` participates in and can host the queued batch
  // indicator (closeable and non-critical-security).
  static bool IsEligibleInfoBar(const infobars::InfoBar* infobar);

  // Returns the batch tracker attached to `manager`, creating it lazily.
  static BatchProgressTracker& GetOrCreate(infobars::InfoBarManager& manager);

  // Returns the batch tracker attached to `manager`, or null if none exists.
  static BatchProgressTracker* Get(const infobars::InfoBarManager* manager);

  BatchProgressTracker(base::PassKey<BatchProgressTracker>,
                       infobars::InfoBarManager& manager);
  BatchProgressTracker(const BatchProgressTracker&) = delete;
  BatchProgressTracker& operator=(const BatchProgressTracker&) = delete;
  ~BatchProgressTracker() override;

  // Updates the currently displayed `host` for the container's settled state
  // and returns the position to display on `host` (or `{0, 0}` if hidden).
  // `has_staying_eligible_bar` indicates whether at least one staying eligible
  // infobar remains in the container (even if `host` is null because a
  // critical-security or non-closeable bar sits below it).
  Position ResolvePosition(const infobars::InfoBar* host,
                           bool has_staying_eligible_bar,
                           size_t pending_count);

 private:
  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerWillBeDestroyed(infobars::InfoBarManager* manager) override;

  void Reset();
  void ResetIfBatchEnded();

  // The tab's `InfoBarManager` that owns `this` via `SupportsUserData`.
  raw_ptr<infobars::InfoBarManager> manager_ = nullptr;

  // The eligible infobar currently hosting the batch indicator.
  raw_ptr<const infobars::InfoBar> active_infobar_ = nullptr;

  // Number of active eligible infobars dismissed so far in the current batch.
  size_t completed_count_ = 0;

  // True while a multi-infobar batch is in progress.
  bool active_ = false;

  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_BATCH_PROGRESS_TRACKER_H_
