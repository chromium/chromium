// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "content/public/browser/navigation_handle.h"

namespace enterprise_net {

EnterpriseProxyTabHelper::EnterpriseProxyTabHelper(
    content::WebContents* web_contents,
    EnterpriseProxyErrorService* error_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<EnterpriseProxyTabHelper>(*web_contents),
      error_service_(error_service) {}

EnterpriseProxyTabHelper::~EnterpriseProxyTabHelper() = default;

void EnterpriseProxyTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    active_navigation_id_ = navigation_handle->GetNavigationId();
  }
}

void EnterpriseProxyTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    if (error_service_) {
      error_service_->RemoveDisguisedError(
          navigation_handle->GetNavigationId());
    }
    if (active_navigation_id_ == navigation_handle->GetNavigationId()) {
      active_navigation_id_ = 0;
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EnterpriseProxyTabHelper);

}  // namespace enterprise_net
