// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace companion {

// Controller for handling views specific logic for the CompanionTabHelper.
class CompanionSidePanelController : public CompanionTabHelper::Delegate,
                                     public content::WebContentsObserver {
 public:
  explicit CompanionSidePanelController(content::WebContents* web_contents);
  CompanionSidePanelController(const CompanionSidePanelController&) = delete;
  CompanionSidePanelController& operator=(const CompanionSidePanelController&) =
      delete;
  ~CompanionSidePanelController() override;

  // CompanionTabHelper::Delegate:
  void CreateAndRegisterEntry() override;
  void DeregisterEntry() override;
  void ShowCompanionSidePanel(
      SidePanelOpenTrigger side_panel_open_trigger) override;
  void UpdateNewTabButton(GURL url_to_open) override;
  void OnCompanionSidePanelClosed() override;
  content::WebContents* GetCompanionWebContentsForTesting() override;

 private:
  std::unique_ptr<views::View> CreateCompanionWebView();
  GURL GetOpenInNewTabUrl();

  // Returns true if the `url` matches the one used by the Search Companion
  // website.
  bool IsSiteTrusted(const GURL& url);

  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  GURL open_in_new_tab_url_;
  const raw_ptr<content::WebContents> web_contents_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_
