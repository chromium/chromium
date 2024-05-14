// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_NAVIGATION_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_NAVIGATION_HELPER_H_

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace lens {

// Class to help have more refined control over user navigation in Lens side
// panel.
class LensSidePanelNavigationHelper
    : public content::WebContentsUserData<LensSidePanelNavigationHelper> {
 public:
  LensSidePanelNavigationHelper(const LensSidePanelNavigationHelper&) = delete;
  LensSidePanelNavigationHelper& operator=(
      const LensSidePanelNavigationHelper&) = delete;
  ~LensSidePanelNavigationHelper() override;

  // Called by NavigationThrottle to open a given URL in a new tab.
  void OpenInNewTab(content::OpenURLParams params);

  // Gets the URL associated with the current image search engine
  const GURL& GetOriginUrl();

  // Maybe installs a throttle for the given navigation. Throttle gives control
  // of user navigation.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

 private:
  friend class content::WebContentsUserData<LensSidePanelNavigationHelper>;
  explicit LensSidePanelNavigationHelper(content::WebContents* web_contents,
                                         Browser* browser,
                                         const std::string& origin_url);

  // Since this helper is tied to the side panel and the side panel is tied to
  // the browser, the browser instance is expected to outlive this helper class.
  raw_ptr<Browser> browser_;

  // The image URL of the search engine associated with this side panel
  // instance.
  GURL origin_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_NAVIGATION_HELPER_H_
