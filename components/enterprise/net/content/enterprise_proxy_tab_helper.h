// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_
#define COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_

#include <stdint.h>

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace enterprise_net {

class EnterpriseProxyErrorService;

// A per-WebContents (per-tab) helper that tracks the active primary main frame
// NavigationID for Enterprise Proxy authentication challenges and cleans up
// error state in EnterpriseProxyErrorService when navigations complete or
// abort.
//
// Concurrently navigating tabs each have their own WebContents and
// EnterpriseProxyTabHelper instance with distinct, globally-unique 64-bit
// NavigationIDs.
class EnterpriseProxyTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<EnterpriseProxyTabHelper> {
 public:
  ~EnterpriseProxyTabHelper() override;

  EnterpriseProxyTabHelper(const EnterpriseProxyTabHelper&) = delete;
  EnterpriseProxyTabHelper& operator=(const EnterpriseProxyTabHelper&) = delete;

  int64_t active_navigation_id() const { return active_navigation_id_; }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit EnterpriseProxyTabHelper(content::WebContents* web_contents,
                                    EnterpriseProxyErrorService* error_service);
  friend class content::WebContentsUserData<EnterpriseProxyTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  raw_ptr<EnterpriseProxyErrorService> error_service_ = nullptr;
  int64_t active_navigation_id_ = 0;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_
