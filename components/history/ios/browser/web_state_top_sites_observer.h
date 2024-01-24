// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_IOS_BROWSER_WEB_STATE_TOP_SITES_OBSERVER_H_
#define COMPONENTS_HISTORY_IOS_BROWSER_WEB_STATE_TOP_SITES_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace history {

class TopSites;

// WebStateTopSitesObserver forwards navigation events from web::WebState to
// TopSites.
class WebStateTopSitesObserver
    : public web::WebStateObserver,
      public web::WebStateUserData<WebStateTopSitesObserver> {
 public:
  WebStateTopSitesObserver(const WebStateTopSitesObserver&) = delete;
  WebStateTopSitesObserver& operator=(const WebStateTopSitesObserver&) = delete;

  ~WebStateTopSitesObserver() override;

 private:
  friend class web::WebStateUserData<WebStateTopSitesObserver>;

  WebStateTopSitesObserver(web::WebState* web_state, TopSites* top_sites);

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Underlying TopSites instance, may be null during testing.
  raw_ptr<TopSites> top_sites_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_IOS_BROWSER_WEB_STATE_TOP_SITES_OBSERVER_H_
