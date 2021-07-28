// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_subresource_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"

namespace safe_browsing {

SafeBrowsingSubresourceTabHelper::~SafeBrowsingSubresourceTabHelper() {}

void SafeBrowsingSubresourceTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    security_interstitials::UnsafeResource resource;
    if (manager_->PopUnsafeResourceForURL(navigation_handle->GetURL(),
                                          &resource)) {
      safe_browsing::SafeBrowsingBlockingPage* blocking_page =
          manager_->blocking_page_factory()->CreateSafeBrowsingPage(
              manager_, navigation_handle->GetWebContents(),
              navigation_handle->GetURL(), {resource},
              /*should_trigger_reporting=*/true);
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(navigation_handle->GetWebContents(),
                                navigation_handle->GetNavigationId(),
                                base::WrapUnique(blocking_page));
    }
  }
}

SafeBrowsingSubresourceTabHelper::SafeBrowsingSubresourceTabHelper(
    content::WebContents* web_contents,
    SafeBrowsingUIManager* manager)
    : WebContentsObserver(web_contents), manager_(manager) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingSubresourceTabHelper)

}  // namespace safe_browsing
