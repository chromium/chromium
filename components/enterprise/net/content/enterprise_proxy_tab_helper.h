// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_
#define COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

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
class EnterpriseProxyTabHelper : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(EnterpriseProxyTabHelper);

  EnterpriseProxyTabHelper(tabs::TabInterface& tab,
                           content::WebContents* web_contents,
                           EnterpriseProxyErrorService* error_service);
  ~EnterpriseProxyTabHelper() override;

  EnterpriseProxyTabHelper(const EnterpriseProxyTabHelper&) = delete;
  EnterpriseProxyTabHelper& operator=(const EnterpriseProxyTabHelper&) = delete;

  static EnterpriseProxyTabHelper* From(tabs::TabInterface* tab);

  int64_t active_navigation_id() const { return active_navigation_id_; }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  raw_ptr<EnterpriseProxyErrorService> error_service_ = nullptr;
  int64_t active_navigation_id_ = 0;
  ui::ScopedUnownedUserData<EnterpriseProxyTabHelper> scoped_unowned_user_data_;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_
