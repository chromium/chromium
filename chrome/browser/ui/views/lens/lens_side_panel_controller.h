// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_view.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
struct OpenURLParams;
}  // namespace content

class BrowserView;
class SidePanel;

namespace lens {

// Controller for the Lens side panel.
class LensSidePanelController : public content::WebContentsObserver,
                                public content::WebContentsDelegate,
                                public views::ViewObserver {
 public:
  LensSidePanelController(base::OnceClosure close_callback,
                          SidePanel* side_panel,
                          BrowserView* browser_view);
  LensSidePanelController(const LensSidePanelController&) = delete;
  LensSidePanelController& operator=(const LensSidePanelController&) = delete;
  ~LensSidePanelController() override;

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void PrimaryPageChanged(content::Page& page) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // Opens the Lens side panel with the given Lens results URL.
  void OpenWithURL(const content::OpenURLParams& params);

  // Loads the Lens website if the side panel view is ready with a width.
  void MaybeLoadURLWithParams();

  // Returns whether the Lens side panel is currently showing.
  bool IsShowing() const;

  // Closes the Lens side panel.
  void Close();

  // Launches the Lens URL in a new tab and closes the side panel.
  void LoadResultsInNewTab();

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

 private:
  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // Handles the close button being clicked.
  void CloseButtonClicked();

  base::OnceClosure close_callback_;
  raw_ptr<SidePanel> side_panel_;
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<lens::LensSidePanelView> side_panel_view_;

  // Copy of the most recent URL params given to the controller.
  std::unique_ptr<content::OpenURLParams> side_panel_url_params_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_CONTROLLER_H_
