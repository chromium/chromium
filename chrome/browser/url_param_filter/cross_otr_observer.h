// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_

#include "base/supports_user_data.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents_observer.h"

namespace url_param_filter {

// Observes navigations that originate in normal browsing and move into OTR
// browsing.
class CrossOtrObserver : public content::WebContentsObserver,
                         public base::SupportsUserData::Data {
 public:
  // The key used to associate this observer with the given WebContents.
  constexpr static const char kUserDataKey[] = "CrossOtrObserver";
  // Attaches the observer in cases where it should do so; leaves `web_contents`
  // unchanged otherwise.
  static void MaybeCreateForWebContents(content::WebContents* web_contents,
                                        const NavigateParams& params);
  explicit CrossOtrObserver(content::WebContents* web_contents);
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  void Detach();
  // Drives state machine logic; we write the cross-OTR response code metric
  // only for the first navigation, which is that which would have parameters
  // filtered.
  bool wrote_response_metric_ = false;
  // Tracks refreshes observed, which could point to an issue with param
  // filtering causing unexpected behavior for the user.
  int refresh_count_ = 0;
};

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_
