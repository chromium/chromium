// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/core/captive_portal_detector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace captive_portal {

const char CaptivePortalDetector::kDefaultURL[] =
    "http://www.gstatic.com/generate_204";

CaptivePortalDetector::CaptivePortalDetector(
    network::mojom::URLLoaderFactory* loader_factory)
    : loader_factory_(loader_factory) {}

CaptivePortalDetector::~CaptivePortalDetector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CaptivePortalDetector::DetectCaptivePortal(
    const GURL& url,
    DetectionCallback detection_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!FetchingURL());
  DCHECK(!detection_callback.is_null());

  if (!detection_callback_.is_null())
    LOG(ERROR) << "DetectCaptivePortal called while request is pending.";
  detection_callback_ = std::move(detection_callback);

  StartProbe(traffic_annotation, url);
}

void CaptivePortalDetector::StartProbe(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  probe_url_ = url;

  // Can't safely use net::LOAD_DISABLE_CERT_NETWORK_FETCHES here,
  // since then the connection may be reused without checking the cert.
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Secure DNS should be disabled for captive portal probes so that when a
  // captive portal is present, the DNS lookup for the probe domain succeeds or
  // is intercepted.
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->disable_secure_dns = true;

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->SetAllowHttpErrorResults(true);
  network::SimpleURLLoader::BodyAsStringCallbackDeprecated callback =
      base::BindOnce(&CaptivePortalDetector::OnSimpleLoaderComplete,
                     base::Unretained(this));
  state_ = State::kProbe;
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory_, std::move(callback));
}

void CaptivePortalDetector::Cancel() {
  simple_loader_.reset();
  detection_callback_.Reset();
  state_ = State::kCancelled;
}

void CaptivePortalDetector::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kProbe);
  CHECK(FetchingURL());
  DCHECK(!detection_callback_.is_null());

  net::HttpResponseHeaders* headers = nullptr;
  int response_code = 0;
  std::optional<size_t> content_length;
  if (simple_loader_->ResponseInfo() &&
      simple_loader_->ResponseInfo()->headers) {
    headers = simple_loader_->ResponseInfo()->headers.get();
    response_code = headers->response_code();
    if (response_body) {
      content_length = response_body->size();
    }
  }
  state_ = State::kCompleted;
  OnSimpleLoaderCompleteInternal(simple_loader_->NetError(), response_code,
                                 content_length, simple_loader_->GetFinalURL(),
                                 headers);
}

void CaptivePortalDetector::OnSimpleLoaderCompleteInternal(
    int net_error,
    int response_code,
    std::optional<size_t> content_length,
    const GURL& url,
    net::HttpResponseHeaders* headers) {
  Results results;
  GetCaptivePortalResultFromResponse(net_error, response_code, content_length,
                                     url, headers, &results);
  simple_loader_.reset();
  std::move(detection_callback_).Run(results);
}

void CaptivePortalDetector::GetCaptivePortalResultFromResponse(
    int net_error,
    int response_code,
    std::optional<size_t> content_length,
    const GURL& url,
    net::HttpResponseHeaders* headers,
    Results* results) const {
  results->result = captive_portal::RESULT_NO_RESPONSE;
  results->response_code = response_code;
  results->retry_after_delta = base::TimeDelta();
  results->landing_url = url;
  results->content_length = content_length;

  VLOG(1) << "Getting captive portal result"
          << " response code: " << results->response_code
          << " content_length: " << results->content_length.value_or(-1)
          << " landing_url: " << results->landing_url;

  // If there's a network error of some sort when fetching a file via HTTP,
  // there may be a networking problem, rather than a captive portal.
  // TODO(mmenke):  Consider special handling for redirects that end up at
  //                errors, especially SSL certificate errors.
  if (net_error != net::OK)
    return;

  // In the case of 503 errors, look for the Retry-After header.
  if (results->response_code == 503) {
    std::string retry_after_string;

    // If there's no Retry-After header, nothing else to do.
    if (!headers->EnumerateHeader(nullptr, "Retry-After", &retry_after_string))
      return;

    base::TimeDelta retry_after_delta;
    if (net::HttpUtil::ParseRetryAfterHeader(
            retry_after_string, GetCurrentTime(), &retry_after_delta)) {
      results->retry_after_delta = retry_after_delta;
    }

    return;
  }

  // A 511 response (Network Authentication Required) means that the user needs
  // to login to whatever server issued the response.
  // See:  http://tools.ietf.org/html/rfc6585
  if (results->response_code == 511) {
    results->result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
    return;
  }

  // Other non-2xx/3xx HTTP responses may indicate server errors.
  if (results->response_code >= 400 || results->response_code < 200)
    return;

  // A 204 response code indicates there's no captive portal.
  if (results->response_code == 204) {
    results->result = captive_portal::RESULT_INTERNET_CONNECTED;
    return;
  }

  // A 200 response code is treated the same as a 204 if there is no content.
  // This is consistent with AOSP and helps support networks that transparently
  // proxy or redirect web content but do not handle 204 content completely
  // correctly. See b/33498325 for an example of a captive portal returning a
  // 200 response code with content.
  // This matches the logic in PortalDetector::ProcessHTTPProbeResult in shill.
  if (results->response_code == 200 && content_length.has_value() &&
      (content_length == 0 || content_length == 1)) {
    results->result = captive_portal::RESULT_INTERNET_CONNECTED;
    return;
  }

  // Otherwise, assume it's a captive portal.
  results->result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
}

base::Time CaptivePortalDetector::GetCurrentTime() const {
  if (time_for_testing_.is_null())
    return base::Time::Now();
  return time_for_testing_;
}

bool CaptivePortalDetector::FetchingURL() const {
  return simple_loader_ != nullptr;
}

}  // namespace captive_portal
