// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {
// BubbleManager is responsible for coordinating showing and hiding bubble
// dialogs for Autofill and Password Manager.
// Multiple bubbles might want to show at the same time (e.g., saving a card,
// updating an address, a password prompt). This manager's job is to ensure that
// only one bubble is visible at any given time and that the most important
// (highest-priority) bubble is the one that gets shown.
//
// It maintains a queue of pending bubble requests and decides which one to
// show based on a defined priority system.
//
// Here's the bubble management algorithm:
//
// === Show Request Logic ===
// When a new bubble (B) requests to be shown:
//
// 1. If no bubble is currently showing:
//    - Show B immediately.
//
// 2. If a bubble (A) is already showing:
//    - Preemption of an active bubble (A) by a new bubble (B) is determined by
//      the following rules, in order:
//      - Force Show: If `force_show` is true, B always preempts A.
//      - Mouse Hover: If the user is hovering over A, B never preempts A.
//      - Same Type: If B and A have the same type, B preempts A only if that
//        type has a "preempt same type" policy.
//      - Priority: Otherwise, B preempts A if priority(B) > priority(A).
//
//    - If B preempts A:
//      - Hide A and add it to the pending queue.
//      - Show B.
//
//    - If B does not preempt A:
//      - Add B to the pending queue. A remains visible.
//
// === Queue and Hiding Logic ===
//
// - When the active bubble is hidden (e.g., closed by the user or preempted):
//   - The manager processes the pending queue to show the next highest-priority
//     bubble.
//
// - The pending queue has the following rules:
//   - It is sorted by priority (descending) and then by request time
//     (ascending).
//   - Only one bubble of a specific type (e.g., kSaveUpdateCard) can be in
//     the queue at any time.
//   - If a request comes in for a bubble type that's already queued:
//     - The new bubble replaces the old one if they have the same type and
//     ShouldAlwaysPreemptSameType returns true, OR if the
//       old one has been in the queue for longer than a timeout
//       (kPendingRequestTimeout).
//     - Otherwise, the new request is discarded.
class BubbleManager {
 public:
  virtual ~BubbleManager() = default;

  static std::unique_ptr<BubbleManager> Create(tabs::TabInterface* tab);
  static BubbleManager* GetForWebContents(content::WebContents* web_contents);
  static BubbleManager* GetForTab(tabs::TabInterface* tab_interface);

  // Requests the bubble for `controller_to_show` to be displayed.
  // If `force_show` is true, this bubble will preempt any active bubble,
  // regardless of priority or hover state.
  virtual void RequestShowController(BubbleControllerBase& controller_to_show,
                                     bool force_show) = 0;

  // Called by the controller when its HideBubble() method is invoked.
  // `show_next_bubble` indicates whether to show the next pending bubble or
  // not.
  virtual void OnBubbleHiddenByController(
      BubbleControllerBase& controller_to_hide,
      bool show_next_bubble) = 0;

  // Returns true if ANY bubble of the specified `bubble_type` is currently
  // in the queue and has not timed out, regardless of preemption policy.
  [[nodiscard]] virtual bool HasPendingBubbleOfSameType(
      const BubbleType bubble_type) const = 0;

  // Returns true if a bubble of the specified `bubble_type` is already
  // pending in the queue and has not timed out.
  // Note: This will always return false for bubble types that preempt
  // themselves (e.g., password bubbles), as they replace existing requests
  // instead of waiting in the queue.
  [[nodiscard]] virtual bool HasConflictingPendingBubble(
      const BubbleType bubble_type) const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_H_
