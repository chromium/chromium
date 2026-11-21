// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_
#define COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace enterprise_net {

// A per-WebContents (per-tab) helper that tracks the active primary main frame
// NavigationID for Enterprise Proxy authentication challenges, redirects to
// static partner error pages when advertised on disguised 407 errors, and
// cleans up error state in EnterpriseProxyErrorService when navigations
// complete or abort.
//
// Concurrently navigating tabs each have their own WebContents and
// EnterpriseProxyTabHelper instance with distinct, globally-unique 64-bit
// NavigationIDs.
class EnterpriseProxyTabHelper : public content::WebContentsObserver,
                                 public EnterpriseProxyErrorService::Observer {
 public:
  DECLARE_USER_DATA(EnterpriseProxyTabHelper);

  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SignIn(content::WebContents* web_contents) = 0;
  };

  EnterpriseProxyTabHelper(tabs::TabInterface& tab,
                           content::WebContents* web_contents,
                           EnterpriseProxyErrorService* error_service,
                           std::unique_ptr<Delegate> delegate = nullptr);
  ~EnterpriseProxyTabHelper() override;

  EnterpriseProxyTabHelper(const EnterpriseProxyTabHelper&) = delete;
  EnterpriseProxyTabHelper& operator=(const EnterpriseProxyTabHelper&) = delete;

  static EnterpriseProxyTabHelper* From(tabs::TabInterface* tab);

  int64_t active_navigation_id() const { return active_navigation_id_; }

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  void SignIn();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // EnterpriseProxyErrorService::Observer:
  void OnDisguisedErrorRecorded(
      int64_t navigation_id,
      const EnterpriseProxyErrorData& error_data) override;
  void OnEnterpriseProxyErrorServiceShuttingDown() override;

 private:
  void RedirectToStaticErrorPage(int64_t failed_navigation_id,
                                 const GURL& redirect_url);

  raw_ptr<EnterpriseProxyErrorService> error_service_ = nullptr;
  std::unique_ptr<Delegate> delegate_;
  // Tracks the currently in-flight primary main-frame navigation and its
  // latest redirect URL so incoming 407 challenges and disguised errors can be
  // matched to this tab. Cleared when the navigation finishes or is replaced
  // by `RedirectToStaticErrorPage()`.
  int64_t active_navigation_id_ = 0;
  GURL active_navigation_url_;
  // Tracks a navigation initiated by `RedirectToStaticErrorPage()` so that if
  // the static error page itself hits a disguised-error `url=`, Chrome falls
  // back to the built-in error interstitial instead of looping.
  int64_t error_redirect_navigation_id_ = 0;
  base::ScopedObservation<EnterpriseProxyErrorService,
                          EnterpriseProxyErrorService::Observer>
      error_service_observation_{this};
  ui::ScopedUnownedUserData<EnterpriseProxyTabHelper> scoped_unowned_user_data_;
  base::WeakPtrFactory<EnterpriseProxyTabHelper> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CONTENT_ENTERPRISE_PROXY_TAB_HELPER_H_
