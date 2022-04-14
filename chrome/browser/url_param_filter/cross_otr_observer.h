// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace url_param_filter {

// Observes navigations that originate in normal browsing and move into OTR
// browsing. This class can be thought of as a state machine:
// start-->blocking-->monitoring-->detached
// where the initial cross-OTR navigation moves to blocking; user activation or
// the start of a second navigation not initiated via client redirect moves to
// monitoring; and the next completed non-refresh navigation after that point,
// regardless of cause, detaches. Note that for our purposes, navigation above
// refers to top-level, main frame navigations only; we do not consider e.g.,
// subframe loads.
class CrossOtrObserver : public content::WebContentsObserver,
                         public content::WebContentsUserData<CrossOtrObserver> {
 public:
  ~CrossOtrObserver() override;

  // Attaches the observer in cases where it should do so; leaves `web_contents`
  // unchanged otherwise.
  static void MaybeCreateForWebContents(content::WebContents* web_contents,
                                        const NavigateParams& params);
  bool IsCrossOtrState();
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  // Inherited from content::WebContentsUserData, but should not be used outside
  // this class or its Android counterpart. MaybeCreateForWebContents should be
  // used instead.
  using content::WebContentsUserData<CrossOtrObserver>::CreateForWebContents;

  base::WeakPtr<CrossOtrObserver> GetWeakPtr();

  void SetDidFilterParams(bool value);

 private:
  explicit CrossOtrObserver(content::WebContents* web_contents);

  friend class content::WebContentsUserData<CrossOtrObserver>;
  // Flushes metrics and removes the observer from the WebContents.
  void Detach();
  // Drives state machine logic; we write the cross-OTR response code metric
  // only for the first navigation, which is that which would have parameters
  // filtered.
  bool observed_response_ = false;
  // Tracks refreshes observed, which could point to an issue with param
  // filtering causing unexpected behavior for the user.
  int refresh_count_ = 0;

  // Whether top-level navigations should have filtering applied. Starts true,
  // and switched to false once a navigation completes and then either:
  // user interaction is observed or a new navigation starts that is not a
  // client redirect.
  bool protecting_navigations_ = true;
  bool did_filter_params_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  base::WeakPtrFactory<CrossOtrObserver> weak_factory_;
};

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_H_
