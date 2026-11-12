// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_web_contents_observer.h"

#include <optional>

#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"

namespace ash {

namespace {

bool IsSslError(content::NavigationHandle* navigation_handle,
                net::Error net_error) {
  if (net_error == net::OK) {
    return false;
  }
  if (net::IsCertificateError(net_error) ||
      net::IsClientCertificateError(net_error)) {
    return true;
  }
  // Keep in sync with SSL/TLS/ECH connection error definitions in
  // net/base/net_error_list.h (primarily in the 100-199 connection error range)
  // that are not already covered by IsCertificateError() (200-299) or
  // IsClientCertificateError().
  switch (net_error) {
    case net::ERR_SSL_PROTOCOL_ERROR:
    case net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
    case net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
    case net::ERR_SSL_RENEGOTIATION_REQUESTED:
    case net::ERR_SSL_NO_RENEGOTIATION:
    case net::ERR_SSL_DECOMPRESSION_FAILURE_ALERT:
    case net::ERR_SSL_BAD_RECORD_MAC_ALERT:
    case net::ERR_SSL_DECRYPT_ERROR_ALERT:
    case net::ERR_SSL_SERVER_CERT_CHANGED:
    case net::ERR_SSL_UNRECOGNIZED_NAME_ALERT:
    case net::ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT:
    case net::ERR_SSL_SERVER_CERT_BAD_FORMAT:
    case net::ERR_SSL_OBSOLETE_CIPHER:
    case net::ERR_EARLY_DATA_REJECTED:
    case net::ERR_WRONG_VERSION_ON_EARLY_DATA:
    case net::ERR_TLS13_DOWNGRADE_DETECTED:
    case net::ERR_SSL_KEY_USAGE_INCOMPATIBLE:
    case net::ERR_INVALID_ECH_CONFIG_LIST:
    case net::ERR_ECH_NOT_NEGOTIATED:
    case net::ERR_ECH_FALLBACK_CERTIFICATE_INVALID:
    case net::ERR_STRICT_ECH_REQUIRED:
      return true;
    default:
      break;
  }
  const std::optional<net::SSLInfo>& ssl_info = navigation_handle->GetSSLInfo();
  return ssl_info.has_value() && net::IsCertStatusError(ssl_info->cert_status);
}

}  // namespace

// Note: Captive portal sign-in logs always use DefaultNetworkIdWithSession()
// because Chrome's traffic is always routed on the default network, except for
// special routing cases such as split VPNs or DHCP option 121 routes which are
// not relevant for captive portal sign-in.

NetworkPortalWebContentsObserver::NetworkPortalWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

NetworkPortalWebContentsObserver::~NetworkPortalWebContentsObserver() = default;

void NetworkPortalWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (response_headers) {
    NET_LOG(EVENT) << DefaultNetworkIdWithSession() << " " << __func__
                   << ": http_status=" << response_headers->response_code()
                   << ", target_url=" << navigation_handle->GetURL();
  } else {
    NET_LOG(EVENT) << DefaultNetworkIdWithSession() << " " << __func__
                   << ": target_url=" << navigation_handle->GetURL();
  }
}

void NetworkPortalWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  const net::Error net_error = navigation_handle->GetNetErrorCode();
  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (IsSslError(navigation_handle, net_error)) {
    NET_LOG(ERROR) << DefaultNetworkIdWithSession() << " " << __func__
                   << ": SSL error: " << net::ErrorToString(net_error) << " ("
                   << net_error << ")";
  } else if (net_error != net::OK) {
    NET_LOG(ERROR) << DefaultNetworkIdWithSession() << " " << __func__
                   << ": navigation error: " << net::ErrorToString(net_error)
                   << " (" << net_error << ")";
  } else if (response_headers) {
    NET_LOG(EVENT) << DefaultNetworkIdWithSession() << " " << __func__
                   << ": http_status=" << response_headers->response_code();
  } else {
    NET_LOG(EVENT) << DefaultNetworkIdWithSession() << " " << __func__
                   << ": http_status=none";
  }
  RequestPortalDetection();
}

void NetworkPortalWebContentsObserver::WebContentsDestroyed() {
  NET_LOG(EVENT) << DefaultNetworkIdWithSession() << " " << __func__;
  RequestPortalDetection();
}

void NetworkPortalWebContentsObserver::RequestPortalDetection() {
  portal_detection_requested_for_testing_++;
  if (NetworkHandler::IsInitialized()) {
    NET_LOG(EVENT) << DefaultNetworkIdWithSession() << " " << __func__;
    NetworkHandler::Get()->network_state_handler()->RequestPortalDetection();
  }
}

}  // namespace ash
