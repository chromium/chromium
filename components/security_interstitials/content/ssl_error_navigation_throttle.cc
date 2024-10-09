// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/buildflag.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "net/cert/cert_status_flags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/guest_view/browser/guest_view_base.h"
#endif

namespace {

// Returns true if `handle`'s navigation is happening in a WebContents
// that uses SSL interstitials. Returns false if a plain error page should be
// used instead.
bool WebContentsUsesInterstitials(content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  if (web_contents == web_contents->GetResponsibleWebContents()) {
    // Outermost contents (e.g. regular tabs) use interstitials.
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromNavigationHandle(handle);
  if (!guest) {
    // Non-guest view inner WebContents should always show error pages instead
    // of interstitials.
    return false;
  }

  // Some guest view types still show SSL interstitials.
  return guest->RequiresSslInterstitials();
#endif
}

}  // namespace

SSLErrorNavigationThrottle::SSLErrorNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    SSLErrorNavigationThrottle::HandleSSLErrorCallback
        handle_ssl_error_callback,
    IsInHostedAppCallback is_in_hosted_app_callback,
    ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttpsCallback
        should_ignore_interstitial_because_navigation_defaulted_to_https_callback)
    : content::NavigationThrottle(navigation_handle),
      handle_ssl_error_callback_(std::move(handle_ssl_error_callback)),
      is_in_hosted_app_callback_(std::move(is_in_hosted_app_callback)),
      should_ignore_interstitial_because_navigation_defaulted_to_https_callback_(
          std::move(
              should_ignore_interstitial_because_navigation_defaulted_to_https_callback)) {
}

SSLErrorNavigationThrottle::~SSLErrorNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
SSLErrorNavigationThrottle::WillFailRequest() {
  content::NavigationHandle* handle = navigation_handle();

  // Check the network error code in case we are here due to a non-ssl related
  // error. SSLInfo also needs to be checked to cover cases where an SSL error
  // does not trigger an interstitial, such as chrome://network-errors.
  if (!net::IsCertificateError(handle->GetNetErrorCode()) ||
      !net::IsCertStatusError(
          handle->GetSSLInfo().value_or(net::SSLInfo()).cert_status)) {
    return content::NavigationThrottle::PROCEED;
  }

  // Do not set special error page HTML for non-primary pages (e.g. regular
  // subframe, prerendering, fenced-frame). Those are handled as normal
  // network errors.
  if (!handle->IsInPrimaryMainFrame() ||
      !WebContentsUsesInterstitials(handle)) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the scheme of this navigation was upgraded to HTTPS (because the user
  // didn't type a scheme), don't show an error.
  // TypedNavigationUpgradeThrottle or HttpsOnlyModeNavigationThrottle will
  // handle the error and fall back to HTTP as needed.
  if (std::move(
          should_ignore_interstitial_because_navigation_defaulted_to_https_callback_)
          .Run(handle)) {
    return content::NavigationThrottle::PROCEED;
  }

  const net::SSLInfo info = handle->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  QueueShowInterstitial(std::move(handle_ssl_error_callback_),
                        handle->GetWebContents(), handle->GetNetErrorCode(),
                        cert_status, info, handle->GetURL());
  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::DEFER);
}

content::NavigationThrottle::ThrottleCheckResult
SSLErrorNavigationThrottle::WillProcessResponse() {
  content::NavigationHandle* handle = navigation_handle();
  // If there was no certificate error, SSLInfo will be empty.
  const net::SSLInfo info = handle->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  if (!net::IsCertStatusError(cert_status)) {
    return content::NavigationThrottle::PROCEED;
  }

  // Do not set special error page HTML for non-primary pages (e.g. regular
  // subframe, prerendering, fenced-frame). Those are handled as normal
  // network errors.
  if (!handle->IsInPrimaryMainFrame() ||
      !WebContentsUsesInterstitials(handle)) {
    return content::NavigationThrottle::PROCEED;
  }

  // Hosted Apps should not be allowed to run if there is a problem with their
  // certificate. So, when a user tries to open such an app, we show an
  // interstitial, even if the user has previously clicked through one. Clicking
  // through the interstitial will continue the navigation in a regular browser
  // window.
  if (std::move(is_in_hosted_app_callback_).Run(handle->GetWebContents())) {
    QueueShowInterstitial(std::move(handle_ssl_error_callback_),
                          handle->GetWebContents(),
                          // The navigation handle's net error code will be
                          // net::OK, because the net stack has allowed the
                          // response to proceed. Synthesize a net error from
                          // the cert status instead.
                          net::MapCertStatusToNetError(cert_status),
                          cert_status, info, handle->GetURL());
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::DEFER);
  }

  return content::NavigationThrottle::PROCEED;
}

const char* SSLErrorNavigationThrottle::GetNameForLogging() {
  return "SSLErrorNavigationThrottle";
}

void SSLErrorNavigationThrottle::QueueShowInterstitial(
    HandleSSLErrorCallback handle_ssl_error_callback,
    content::WebContents* web_contents,
    int net_error,
    int cert_status,
    const net::SSLInfo& ssl_info,
    const GURL& request_url) {
  // It is safe to call this without posting because SSLErrorHandler will always
  // call ShowInterstitial asynchronously, giving the throttle time to defer the
  // navigation.
  std::move(handle_ssl_error_callback)
      .Run(web_contents, net_error, ssl_info, request_url,
           base::BindOnce(&SSLErrorNavigationThrottle::ShowInterstitial,
                          weak_ptr_factory_.GetWeakPtr(), net_error));
}

void SSLErrorNavigationThrottle::ShowInterstitial(
    int net_error,
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page) {
  // Get the error page content before giving up ownership of |blocking_page|.
  std::string error_page_content = blocking_page->GetHTMLContents();

  content::NavigationHandle* handle = navigation_handle();

  // Do not display insterstitials for SSL errors from non-primary pages (e.g.
  // prerendering, fenced-frame). For prerendering specifically, we
  // should already have canceled the prerender from OnSSLCertificateError
  // before the throttle runs.
  DCHECK(handle->IsInPrimaryMainFrame());

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      handle, std::move(blocking_page));

  CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, static_cast<net::Error>(net_error),
      error_page_content));
}
