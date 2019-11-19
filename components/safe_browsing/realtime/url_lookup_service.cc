// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/url_lookup_service.h"

#include "base/base64url.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

const char kRealTimeLookupUrlPrefix[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/realtime";

const size_t kMaxFailuresToEnforceBackoff = 3;

const size_t kBackOffResetDurationInSeconds = 5 * 60;  // 5 minutes.

const size_t kURLLookupTimeoutDurationInSeconds = 1 * 60;  // 1 minute.

// Fragements, usernames and passwords are removed, becuase fragments are only
// used for local navigations and usernames/passwords are too privacy sensitive.
GURL SanitizeURL(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}

}  // namespace

RealTimeUrlLookupService::RealTimeUrlLookupService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

void RealTimeUrlLookupService::StartLookup(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(url.is_valid());

  std::unique_ptr<RTLookupRequest> request = FillRequestProto(url);

  std::string req_data;
  request->SerializeToString(&req_data);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_realtime_url_lookup",
                                          R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing can't detect that a URL is safe based on its "
            "local database, it sends the top-level URL to Google to verify it "
            "before showing a warning to the user."
          trigger:
            "When a main frame URL fails to match the local hash-prefix "
            "database of known safe URLs and a valid result from a prior "
            "lookup is not already cached, this will be sent."
          data: "The main frame URL that did not match the local safelist."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing real time URL checks by "
            "unchecking 'Protect you and your device from dangerous sites' in "
            "Chromium settings under Privacy, or by unchecking 'Make searches "
            "and browsing better (Sends URLs of pages you visit to Google)' in "
            "Chromium settings under Privacy."
          chrome_policy {
            SafeBrowsingRealTimeLookupEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingRealTimeLookupEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kRealTimeLookupUrlPrefix);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";

  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* loader = owned_loader.get();
  owned_loader->AttachStringForUpload(req_data, "application/octet-stream");
  owned_loader->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kURLLookupTimeoutDurationInSeconds));
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RealTimeUrlLookupService::OnURLLoaderComplete,
                     GetWeakPtr(), loader));

  pending_requests_[owned_loader.release()] = std::move(response_callback);

  std::move(request_callback).Run(std::move(request));
}

RealTimeUrlLookupService::~RealTimeUrlLookupService() {
  for (auto& pending : pending_requests_) {
    // An empty response is treated as safe.
    auto response = std::make_unique<RTLookupResponse>();
    std::move(pending.second).Run(std::move(response));
    delete pending.first;
  }
  pending_requests_.clear();
}

void RealTimeUrlLookupService::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto it = pending_requests_.find(url_loader);
  DCHECK(it != pending_requests_.end()) << "Request not found";

  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      "SafeBrowsing.RT.Network.Result", net_error, response_code);

  auto response = std::make_unique<RTLookupResponse>();
  bool success = (net_error == net::OK) && (response_code == net::HTTP_OK) &&
                 response->ParseFromString(*response_body);
  success ? HandleLookupSuccess() : HandleLookupError();

  std::move(it->second).Run(std::move(response));
  delete it->first;
  pending_requests_.erase(it);
}

bool RealTimeUrlLookupService::CanCheckUrl(const GURL& url) const {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (net::IsLocalhost(url)) {
    // Includes: "//localhost/", "//localhost.localdomain/", "//127.0.0.1/"
    return false;
  }

  net::IPAddress ip_address;
  if (url.HostIsIPAddress() && ip_address.AssignFromIPLiteral(url.host()) &&
      !ip_address.IsPubliclyRoutable()) {
    // Includes: "//192.168.1.1/", "//172.16.2.2/", "//10.1.1.1/"
    return false;
  }

  return true;
}

std::unique_ptr<RTLookupRequest> RealTimeUrlLookupService::FillRequestProto(
    const GURL& url) {
  auto request = std::make_unique<RTLookupRequest>();
  request->set_url(SanitizeURL(url).spec());
  request->set_lookup_type(RTLookupRequest::NAVIGATION);
  // TODO(crbug.com/1017499): Set ChromeUserPopulation.
  return request;
}

void RealTimeUrlLookupService::ExitBackoff() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ResetFailures();
}

void RealTimeUrlLookupService::HandleLookupError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  consecutive_failures_++;

  if (IsInBackoffMode()) {
    reset_backoff_timer_.Stop();
    reset_backoff_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kBackOffResetDurationInSeconds),
        this, &RealTimeUrlLookupService::ExitBackoff);
  }
}

void RealTimeUrlLookupService::HandleLookupSuccess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ResetFailures();
}

bool RealTimeUrlLookupService::IsInBackoffMode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return consecutive_failures_ >= kMaxFailuresToEnforceBackoff;
}

void RealTimeUrlLookupService::ResetFailures() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  consecutive_failures_ = 0;
  reset_backoff_timer_.Stop();
}

// static
SBThreatType RealTimeUrlLookupService::GetSBThreatTypeForRTThreatType(
    RTLookupResponse::ThreatInfo::ThreatType rt_threat_type) {
  switch (rt_threat_type) {
    case RTLookupResponse::ThreatInfo::WEB_MALWARE:
      return SB_THREAT_TYPE_URL_MALWARE;
    case RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case RTLookupResponse::ThreatInfo::UNCLEAR_BILLING:
      return SB_THREAT_TYPE_BILLING;
    case RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unexpected RTLookupResponse::ThreatType encountered";
      return SB_THREAT_TYPE_SAFE;
  }
}

base::WeakPtr<RealTimeUrlLookupService> RealTimeUrlLookupService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
