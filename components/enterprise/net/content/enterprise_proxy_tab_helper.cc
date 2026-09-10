// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"

namespace enterprise_net {

DEFINE_USER_DATA(EnterpriseProxyTabHelper);

EnterpriseProxyTabHelper::EnterpriseProxyTabHelper(
    tabs::TabInterface& tab,
    content::WebContents* web_contents,
    EnterpriseProxyErrorService* error_service,
    std::unique_ptr<Delegate> delegate)
    : content::WebContentsObserver(web_contents),
      error_service_(error_service),
      delegate_(std::move(delegate)),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

EnterpriseProxyTabHelper::~EnterpriseProxyTabHelper() = default;

// static
EnterpriseProxyTabHelper* EnterpriseProxyTabHelper::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

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

void EnterpriseProxyTabHelper::SignIn() {
  if (delegate_ && web_contents()) {
    delegate_->SignIn(web_contents());
  }
}

}  // namespace enterprise_net
