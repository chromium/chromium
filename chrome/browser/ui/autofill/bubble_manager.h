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
