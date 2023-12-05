// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class BrowserView;
class AboutThisSiteSidePanelView;
class SidePanelUI;

namespace views {
class View;
} // namespace views

// AboutThisSideSidePanelCoordinator handles the creation and registration of
// the AboutThisSidePanelView.
class AboutThisSideSidePanelCoordinator
    : public content::WebContentsUserData<AboutThisSideSidePanelCoordinator>,
      public content::WebContentsObserver {
 public:
  explicit AboutThisSideSidePanelCoordinator(
      content::WebContents* web_contents);
  AboutThisSideSidePanelCoordinator(const AboutThisSideSidePanelCoordinator&) =
      delete;
  AboutThisSideSidePanelCoordinator& operator=(
      const AboutThisSideSidePanelCoordinator&) = delete;
  ~AboutThisSideSidePanelCoordinator() override;

  // Registers ATS entry in the side panel but does not show it.
  void RegisterEntry(const GURL& more_about_url);

  // Registers ATS entry in the side panel and shows side panel with ATS
  // selected if its not shown.
  void RegisterEntryAndShow(const GURL& more_about_url);

 private:
  friend class content::WebContentsUserData<AboutThisSideSidePanelCoordinator>;

  BrowserView* GetBrowserView() const;

  SidePanelUI* GetSidePanelUI();

  // Called when SidePanel is opened.
  std::unique_ptr<views::View> CreateAboutThisSiteWebView();

  // Called to get the URL for the "open in new tab" button.
  GURL GetOpenInNewTabUrl();

  // content::WebContentsObserver:
  // Override DidFinishNavigation to ensure that the AboutThisSide side panel
  // is closed or updates when the user navigates to a different site and
  // that cached data is cleaned up.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Stores the |url_params| for the AbouThisSide SidePanel and the
  // |context_url| that they are associated with.
  struct URLInfo {
    // URL of the page this side panel is related to.
    GURL context_url;
    // URL of the side panel button for opening its content in a new tab.
    GURL new_tab_url;
    // Parameters for opening the side panel.
    content::OpenURLParams url_params;
  };

  // Stores the OpenURLParams that were last registered and the URL of the
  // site that these params belong to.
  std::optional<URLInfo> last_url_info_;

  // Stores whether a SidePanel entry has been shown yet or is just registered
  // at pageload. Used to differentiate SidePanels previously opened or opened
  // from PageInfo from panels opened directly through the SidePanel dropdown.
  bool registered_but_not_shown_ = false;

  base::WeakPtr<AboutThisSiteSidePanelView> about_this_site_side_panel_view_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_COORDINATOR_H_
