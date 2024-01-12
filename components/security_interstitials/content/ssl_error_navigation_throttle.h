// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/navigation_throttle.h"
#include "net/ssl/ssl_info.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace security_interstitials {
class SecurityInterstitialPage;
}  // namespace security_interstitials

// SSLErrorNavigationThrottle watches for failed navigations that should be
// displayed as SSL interstitial pages. More specifically,
// SSLErrorNavigationThrottle::WillFailRequest() will defer any navigations that
// failed due to a certificate error. After calculating which interstitial to
// show, it will cancel the navigation with the interstitial's custom error page
// HTML.
class SSLErrorNavigationThrottle : public content::NavigationThrottle {
 public:
  typedef base::OnceCallback<void(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      base::OnceCallback<void(
          std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
          blocking_page_ready_callback)>
      HandleSSLErrorCallback;

  // Returns whether |web_contents| is in the context of a hosted app, as the
  // logic of when to display interstitials for SSL errors is specialized for
  // hosted apps. This is exposed as a callback because although the WebContents
  // is known at the time of creating SSLErrorNavigationThrottle, it may not
  // have been inserted into a browser by the time the navigation begins. See
  // browser_navigator.cc.
  typedef base::OnceCallback<bool(content::WebContents* web_contents)>
      IsInHostedAppCallback;

  typedef base::OnceCallback<bool(content::NavigationHandle* handle)>
      ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttpsCallback;

  SSLErrorNavigationThrottle(
      content::NavigationHandle* handle,
      HandleSSLErrorCallback handle_ssl_error_callback,
      IsInHostedAppCallback is_in_hosted_app_callback,
      ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttpsCallback
          should_ignore_interstitial_because_navigation_defaulted_to_https_callback);
  ~SSLErrorNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  void QueueShowInterstitial(HandleSSLErrorCallback handle_ssl_error_callback,
                             content::WebContents* web_contents,
                             int net_error,
                             int cert_status,
                             const net::SSLInfo& ssl_info,
                             const GURL& request_url);
  void ShowInterstitial(
      int net_error,
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          blocking_page);

  HandleSSLErrorCallback handle_ssl_error_callback_;
  IsInHostedAppCallback is_in_hosted_app_callback_;
  ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttpsCallback
      should_ignore_interstitial_because_navigation_defaulted_to_https_callback_;
  base::WeakPtrFactory<SSLErrorNavigationThrottle> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_NAVIGATION_THROTTLE_H_
