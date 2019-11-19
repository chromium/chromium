// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/buildflag.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "net/cert/cert_status_flags.h"

SSLErrorNavigationThrottle::SSLErrorNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    SSLErrorNavigationThrottle::HandleSSLErrorCallback
        handle_ssl_error_callback,
    IsInHostedAppCallback is_in_hosted_app_callback)
    : content::NavigationThrottle(navigation_handle),
      ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      handle_ssl_error_callback_(std::move(handle_ssl_error_callback)),
      is_in_hosted_app_callback_(std::move(is_in_hosted_app_callback)) {}

SSLErrorNavigationThrottle::~SSLErrorNavigationThrottle() {}

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

  // Do not set special error page HTML for subframes; those are handled as
  // normal network errors.
  if (!handle->IsInMainFrame()) {
    return content::NavigationThrottle::PROCEED;
  }

  const net::SSLInfo info = handle->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  QueueShowInterstitial(std::move(handle_ssl_error_callback_),
                        handle->GetWebContents(), handle->GetNetErrorCode(),
                        cert_status, info, handle->GetURL(),
                        std::move(ssl_cert_reporter_));
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

  // Do not set special error page HTML for subframes; those are handled as
  // normal network errors.
  if (!handle->IsInMainFrame()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Hosted Apps should not be allowed to run if there is a problem with their
  // certificate. So, when a user tries to open such an app, we show an
  // interstitial, even if the user has previously clicked through one. Clicking
  // through the interstitial will continue the navigation in a regular browser
  // window.
  if (std::move(is_in_hosted_app_callback_).Run(handle->GetWebContents())) {
    QueueShowInterstitial(
        std::move(handle_ssl_error_callback_), handle->GetWebContents(),
        // The navigation handle's net error code will be
        // net::OK, because the net stack has allowed the
        // response to proceed. Synthesize a net error from
        // the cert status instead.
        net::MapCertStatusToNetError(cert_status), cert_status, info,
        handle->GetURL(), std::move(ssl_cert_reporter_));
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
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  // It is safe to call this without posting because SSLErrorHandler will always
  // call ShowInterstitial asynchronously, giving the throttle time to defer the
  // navigation.
  std::move(handle_ssl_error_callback)
      .Run(web_contents, net_error, ssl_info, request_url,
           std::move(ssl_cert_reporter),
           base::Callback<void(content::CertificateRequestResultType)>(),
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
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      handle->GetWebContents(), handle->GetNavigationId(),
      std::move(blocking_page));

  CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, static_cast<net::Error>(net_error),
      error_page_content));
}
