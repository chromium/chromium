// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/ios/browser/web_state_top_sites_observer.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/history/core/browser/top_sites.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace history {

WebStateTopSitesObserver::WebStateTopSitesObserver(web::WebState* web_state,
                                                   TopSites* top_sites)
    : top_sites_(top_sites) {
  web_state->AddObserver(this);
}

WebStateTopSitesObserver::~WebStateTopSitesObserver() {
}

void WebStateTopSitesObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // TODO(crbug.com/41441240): Remove GetLastCommittedItem nil check once
  // HasComitted has been fixed.
  if (top_sites_ && navigation_context->HasCommitted() &&
      web_state->GetNavigationManager()->GetLastCommittedItem()) {
    top_sites_->OnNavigationCommitted(
        web_state->GetNavigationManager()->GetLastCommittedItem()->GetURL());
  }
}

void WebStateTopSitesObserver::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(WebStateTopSitesObserver)

}  // namespace history
