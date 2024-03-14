// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

class Browser;
class GURL;
class LensOverlayController;
class SidePanelUI;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
class WebView;
}  // namespace views

namespace lens {

// Handles the creation and registration of the lens overlay side panel entry.
class LensOverlaySidePanelCoordinator : public SidePanelEntryObserver {
 public:
  LensOverlaySidePanelCoordinator(
      Browser* browser,
      LensOverlayController* lens_overlay_controller,
      SidePanelUI* side_panel_ui,
      content::WebContents* web_contents);
  LensOverlaySidePanelCoordinator(const LensOverlaySidePanelCoordinator&) =
      delete;
  LensOverlaySidePanelCoordinator& operator=(
      const LensOverlaySidePanelCoordinator&) = delete;
  ~LensOverlaySidePanelCoordinator() override;

  // Registers the side panel entry in the side panel if it doesn't already
  // exist and then shows it.
  void RegisterEntryAndShow();

  // SidePanelEntryObserver:
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Registers the entry in the side panel if it doesn't already exist.
  void RegisterEntry();

  // Deregisters the entry in the side panel if it exists.
  void DeregisterEntry();

  // Called to get the URL for the "open in new tab" button.
  GURL GetOpenInNewTabUrl();

  // Gets the tab web contents where this side panel was opened.
  content::WebContents* GetTabWebContents();

  std::unique_ptr<views::View> CreateLensOverlayResultsView();

  // The browser of the tab web contents passed by the overlay.
  const raw_ptr<Browser> tab_browser_;

  // Owns this.
  const raw_ptr<LensOverlayController> lens_overlay_controller_;

  // The side panel UI corresponding to the tab's browser.
  const raw_ptr<SidePanelUI> side_panel_ui_;

  base::WeakPtr<content::WebContents> tab_web_contents_;
  raw_ptr<views::WebView> side_panel_web_view_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
