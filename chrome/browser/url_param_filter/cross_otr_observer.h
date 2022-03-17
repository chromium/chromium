// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_

#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace url_param_filter {

// Observes navigations that originate in normal browsing and move into OTR
// browsing.
class CrossOtrObserver : public content::WebContentsObserver,
                         public content::WebContentsUserData<CrossOtrObserver> {
 public:
  // Attaches the observer in cases where it should do so; leaves `web_contents`
  // unchanged otherwise.
  static void MaybeCreateForWebContents(content::WebContents* web_contents,
                                        const NavigateParams& params);
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  explicit CrossOtrObserver(content::WebContents* web_contents);

  friend class content::WebContentsUserData<CrossOtrObserver>;
  // Inherited from content::WebContentsUserData, but should not be used outside
  // this class. MaybeCreateForWebcontents must be used instead.
  using content::WebContentsUserData<CrossOtrObserver>::CreateForWebContents;
  // Flushes metrics and removes the observer from the WebContents.
  void Detach();
  // Drives state machine logic; we write the cross-OTR response code metric
  // only for the first navigation, which is that which would have parameters
  // filtered.
  bool wrote_response_metric_ = false;
  // Tracks refreshes observed, which could point to an issue with param
  // filtering causing unexpected behavior for the user.
  int refresh_count_ = 0;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_
