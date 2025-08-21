// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ref.h"

namespace content {
class WebContents;
}

namespace autofill {

class BubbleControllerBase;

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
//    - The manager checks if B should preempt A. Preemption happens if:
//      a) priority(B) > priority(A), OR
//      b) Both A and B are password bubbles.
//    - HOWEVER, preemption is blocked if the user is currently hovering their
//      mouse over bubble A.
//
//    - If B preempts A:
//      - Hide A and add it to the pending queue.
//      - Show B.
//
//    - If B does not preempt A (lower/equal priority, or A is hovered):
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
//     - The new bubble replaces the old one if it's a password bubble OR if
//       the old one has been in the queue for longer than a timeout
//       (kPendingRequestTimeout).
//     - Otherwise, the new request is discarded.
class BubbleManager {
 public:
  virtual ~BubbleManager() = default;

  static std::unique_ptr<BubbleManager> Create();
  static BubbleManager* GetForWebContents(content::WebContents* web_contents);

  // Called by the bubbles once they are ready to be shown.
  virtual void RequestShowController(
      BubbleControllerBase& controller_to_show) = 0;

  // Called by the controller when its HideBubble() method is invoked.
  virtual void OnBubbleHiddenByController(
      BubbleControllerBase& controller_to_hide) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_H_
