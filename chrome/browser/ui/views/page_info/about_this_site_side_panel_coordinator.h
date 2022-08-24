// Copyright 2022 The Chromium Authors. All rights reserved.
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
  void RegisterEntry(const content::OpenURLParams& params);

  // Registers ATS entry in the side panel and shows side panel with ATS
  // selected if its not shown.
  void RegisterEntryAndShow(const content::OpenURLParams& params);

 private:
  friend class content::WebContentsUserData<AboutThisSideSidePanelCoordinator>;

  BrowserView* GetBrowserView() const;

  // Called when SidePanel is opened.
  std::unique_ptr<views::View> CreateAboutThisSiteWebView();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Stores the URL open params that were last requested. Used to
  // open a SidePanel to the right URL in case it got destroyed and needs to
  // be recreated.
  absl::optional<content::OpenURLParams> last_url_params_;
  // Stores whether a SidePanel entry has been shown yet or is just registered
  // at pageload. Used to differentiate SidePanels previously opened or opened
  // from PageInfo from panels opened directly through the SidePanel dropdown.
  bool registered_but_not_shown_ = false;

  base::WeakPtr<AboutThisSiteSidePanelView> about_this_site_side_panel_view_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_COORDINATOR_H_
