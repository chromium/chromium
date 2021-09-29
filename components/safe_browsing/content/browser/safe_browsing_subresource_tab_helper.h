// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {

class SafeBrowsingUIManager;

class SafeBrowsingSubresourceTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SafeBrowsingSubresourceTabHelper> {
 public:
  SafeBrowsingSubresourceTabHelper(const SafeBrowsingSubresourceTabHelper&) =
      delete;
  SafeBrowsingSubresourceTabHelper& operator=(
      const SafeBrowsingSubresourceTabHelper&) = delete;

  ~SafeBrowsingSubresourceTabHelper() override;

  // WebContentsObserver::
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<SafeBrowsingSubresourceTabHelper>;

  SafeBrowsingSubresourceTabHelper(content::WebContents* web_contents,
                                   SafeBrowsingUIManager* manager);

  SafeBrowsingUIManager* manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_
