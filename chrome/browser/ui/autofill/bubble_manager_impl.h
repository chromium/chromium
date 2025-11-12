// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_IMPL_H_

#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"

namespace base {
class CallbackListSubscription;
}  // namespace base

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class BubbleManagerImpl : public BubbleManager {
 public:
  explicit BubbleManagerImpl(tabs::TabInterface* tab);
  ~BubbleManagerImpl() override;

  BubbleManagerImpl(const BubbleManagerImpl&) = delete;
  BubbleManagerImpl& operator=(const BubbleManagerImpl&) = delete;

  // BubbleManager:
  void RequestShowController(BubbleControllerBase& controller_to_show,
                             bool force_show) override;
  void OnBubbleHiddenByController(BubbleControllerBase& controller_to_hide,
                                  bool show_next_bubble) override;
  bool HasPendingBubbleOfSameType(const BubbleType bubble_type) const override;
  bool HasConflictingPendingBubble(const BubbleType bubble_type) const override;

 private:
  struct PendingRequest {
    PendingRequest(base::WeakPtr<BubbleControllerBase> controller,
                   base::TimeTicks time_added,
                   int priority);
    ~PendingRequest();
    PendingRequest(const PendingRequest& other);
    PendingRequest& operator=(const PendingRequest& other);

    // Sorts by priority (descending), then by time (ascending) as a
    // tie-breaker.
    bool operator<(const PendingRequest& other) const;

    base::WeakPtr<BubbleControllerBase> controller;
    base::TimeTicks time_added;
    int priority;
  };

  // Checks the pending bubbles queue and shows the highest-priority one if no
  // bubble is currently active.
  void ProcessPendingBubbles(bool tab_entered_foreground);

  // Shows the given controller, sets it as the active one, and ensures
  // it's removed from the pending queue.
  void ShowAndSetCurrentActive(
      base::WeakPtr<BubbleControllerBase> controller_to_show);

  // Adds a controller to the pending queue based on (uniqueness by type,
  // timeout, and password exception).
  void AddToPendingQueue(base::WeakPtr<BubbleControllerBase> controller);

  // Hides the currently active bubble to show a higher-priority one.
  void HideActiveBubbleForPreemption();

  // Returns true if the `new_controller` should replace the
  // `active_bubble_controller_`.
  // 1. Certain bubbles always replace an existing one of similar type (e.g.
  // passwords).
  // 2. Any bubble with a higher priority replaces the active one.
  bool ShouldReplaceExistingBubble(const BubbleType new_bubble_type) const;

  // tabs::TabInterface related overrides:
  void TabWillEnterBackground(tabs::TabInterface* tab_interface);
  void TabDidEnterForeground(tabs::TabInterface* tab_interface);

  // Currently active controller that is shown.
  base::WeakPtr<BubbleControllerBase> active_bubble_controller_ = nullptr;

  // A queue of controllers that have requested to be shown. The container is
  // kept sorted by priority and creation time.
  std::set<PendingRequest> pending_bubbles_queue_;

  // A boolean indicating that the manager is in the process of showing a
  // bubble. This could mean another bubble is in the process of preemption.
  bool handling_show_request_ = false;

  // A boolean indicating that the manager is in the process of hiding the
  // bubble since the tab is to enter the background.
  bool handling_tab_will_enter_background_request_ = false;

  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  raw_ptr<tabs::TabInterface> tab_interface_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_IMPL_H_
