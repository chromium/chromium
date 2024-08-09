// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

class LensOverlayController;
class RealboxHandler;
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

  // Shows an instance of the lens search bubble for this browser. This will
  // only be called by the LensOverlayController.
  void Show();

  // Closes the search bubble and cleans up the reference that the
  // RealboxOmniboxClient has of web_contents_. This will only be called by the
  // LensOverlayController.
  void Close();

  // Returns whether the search bubble is open.
  bool IsSearchBubbleVisible();

  // Closes the lens overlay. This is called when the user closes the bubble
  // manually through the "X" button.
  void CloseLensOverlay();

  // Removes LensOverlayControllerGlue from the web contents.
  void RemoveLensOverlayControllerGlue();

  // This method is used to set up communication between this instance and the
  // search bubble searchbox WebUI Passes
  //  ownership of `handler` to the search_bubble_controller_.
  void SetContextualSearchboxHandler(std::unique_ptr<RealboxHandler> handler);

  content::WebContents* search_bubble_web_contents_for_testing() {
    return web_contents_;
  }

  const WebUIBubbleDialogView* bubble_view_for_testing() {
    return bubble_view_.get();
  }

 private:
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  // Owns this.
  raw_ptr<LensOverlayController> lens_overlay_controller_;

  // Contextual searchbox handler. The handler is null if the WebUI containing
  // the searchbox has not been initialized yet, like in the case of the search
  // bubble opening. In addition, the handler may be initialized, but the
  // remote not yet set because the WebUI calls SetPage() once it is ready to
  // receive data from C++. Therefore, we must always check that:
  //      1) contextual_searchbox_handler_ exists and
  //      2) contextual_searchbox_handler_->IsRemoteBound() is true.
  std::unique_ptr<RealboxHandler> contextual_searchbox_handler_;

  raw_ptr<content::WebContents> web_contents_;

  // Must be the last member.
  base::WeakPtrFactory<LensSearchBubbleController> weak_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
