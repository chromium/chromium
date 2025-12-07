// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_RESULTS_PANEL_ROUTER_H_
#define CHROME_BROWSER_UI_LENS_LENS_RESULTS_PANEL_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "content/public/browser/web_contents.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

namespace content {
class WebContents;
}  // namespace content

namespace lens {

class LensOverlaySidePanelCoordinator;

// A router for the results panel that Lens should load results into,
class LensResultsPanelRouter {
 public:
  explicit LensResultsPanelRouter(LensSearchController* lens_search_controller);
  ~LensResultsPanelRouter();

  // Whether the results panel entry is currently the active entry in the side
  // panel UI.
  bool IsEntryShowing();

  // Returns the panel type of the results panel.
  SidePanelEntry::PanelType GetPanelType() const;

  // Focuses the searchbox in the results panel.
  void FocusSearchbox();

  // Called when the overlay is shown.
  void OnOverlayShown();

  // Called when the overlay is hidden.
  void OnOverlayHidden();

 private:
  tabs::TabInterface* tab_interface() const {
    return lens_search_controller_->GetTabInterface();
  }

  content::WebContents* web_contents() const {
    return tab_interface()->GetContents();
  }

  raw_ptr<LensOverlaySidePanelCoordinator> lens_side_panel_coordinator() const {
    return lens_search_controller_->lens_overlay_side_panel_coordinator();
  }

  // Owns this.
  raw_ptr<LensSearchController> lens_search_controller_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_RESULTS_PANEL_ROUTER_H_
