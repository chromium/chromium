// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class GURL;
class SideSearchSideContentsHelper;

// Side Search helper for the WebContents hosted in the browser's main tab area.
class SideSearchTabContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SideSearchTabContentsHelper> {
 public:
  ~SideSearchTabContentsHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Gets the `side_panel_contents_` for the tab. Creates one if it does not
  // currently exist.
  content::WebContents* GetSidePanelContents();

 private:
  friend class content::WebContentsUserData<SideSearchTabContentsHelper>;
  explicit SideSearchTabContentsHelper(content::WebContents* web_contents);

  // Gets the helper for the side contents.
  SideSearchSideContentsHelper* GetSideContentsHelper();

  // Creates the `side_panel_contents_` associated with this helper's tab
  // contents.
  void CreateSidePanelContents();

  // The last Google search URL encountered by this tab contents.
  absl::optional<GURL> last_search_url_;

  // The side panel contents associated with this tab contents.
  // TODO(tluk): Update the way we manage the `side_panel_contents_` to avoid
  // keeping the object around when not needed by the feature.
  std::unique_ptr<content::WebContents> side_panel_contents_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_
