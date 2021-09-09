// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_CONTROLLER_H_

#include "content/public/browser/web_contents_observer.h"

namespace content {
struct OpenURLParams;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

class BrowserView;
class SidePanel;

namespace lens {

// Controller for the Lens side panel.
class LensSidePanelController : public content::WebContentsObserver {
 public:
  LensSidePanelController(SidePanel* side_panel, BrowserView* browser_view);
  LensSidePanelController(const LensSidePanelController&) = delete;
  LensSidePanelController& operator=(const LensSidePanelController&) = delete;
  ~LensSidePanelController() override;

  // Opens the Lens side panel with the given Lens results URL.
  void OpenWithURL(const content::OpenURLParams& params);

  // Closes the Lens side panel.
  void Close();

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

  SidePanel* side_panel_;
  BrowserView* browser_view_;
  views::WebView* side_panel_webview_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_CONTROLLER_H_
