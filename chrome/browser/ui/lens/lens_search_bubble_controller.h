// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

class LensOverlayController;
class WebUIBubbleDialogView;

namespace lens {

// Manages the SearchBubble instance for a lens overlay.
class LensSearchBubbleController {
 public:
  explicit LensSearchBubbleController(
      LensOverlayController* lens_overlay_controller);
  LensSearchBubbleController(const LensSearchBubbleController&) = delete;
  LensSearchBubbleController& operator=(const LensSearchBubbleController&)
    = delete;
  ~LensSearchBubbleController();

  // Shows an instance of the lens search bubble for this browser.
  void Show();
  // Closes the instance of the lens search bubble.
  void Close();

  const WebUIBubbleDialogView* bubble_view_for_testing() {
    return bubble_view_.get();
  }

 private:
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  // Owns this.
  raw_ptr<LensOverlayController> lens_overlay_controller_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
