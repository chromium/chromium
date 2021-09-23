// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}  // namespace content

class GURL;

// Side Search helper for the WebContents hosted in the side panel.
class SideSearchSideContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SideSearchSideContentsHelper> {
 public:
  class Delegate {
   public:
    // Called by the side contents helper to navigate its associated tab
    // contents.
    virtual void NavigateInTabContents(
        const content::OpenURLParams& params) = 0;

    // Called when the last search URL encountered by the side panel has been
    // updated.
    virtual void LastSearchURLUpdated(const GURL& url) = 0;
  };

  ~SideSearchSideContentsHelper() override;

  // Maybe installs a throttle for the given navigation.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Navigates the associated tab contents to `url`.
  void NavigateInTabContents(const content::OpenURLParams& params);

  // Loads the `url` in the side contents, applying any additional headers as
  // necessary.
  void LoadURL(const GURL& url);

  // Called to set the tab contents associated with this side panel contents.
  // The tab contents will always outlive this helper and its associated side
  // contents.
  void SetDelegate(Delegate* delegate);

 private:
  friend class content::WebContentsUserData<SideSearchSideContentsHelper>;
  explicit SideSearchSideContentsHelper(content::WebContents* web_contents);

  // `delegate_` will outlive the SideContentsWrapper.
  Delegate* delegate_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_
