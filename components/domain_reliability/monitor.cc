// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/domain_reliability/monitor.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/google_configs.h"
#include "components/domain_reliability/quic_error_mapping.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace domain_reliability {

namespace {

// Creates a new beacon based on |beacon_template| but fills in the status,
// chrome_error, and server_ip fields based on the endpoint and result of
// |attempt|.
//
// If there is no matching status for the result, returns nullptr (which
// means the attempt should not result in a beacon being reported).
std::unique_ptr<DomainReliabilityBeacon> CreateBeaconFromAttempt(
    const DomainReliabilityBeacon& beacon_template,
    const net::ConnectionAttempt& attempt) {
  std::string status;
  if (!GetDomainReliabilityBeaconStatus(
          attempt.result, beacon_template.http_response_code, &status)) {
    return nullptr;
  }

  auto beacon = std::make_unique<DomainReliabilityBeacon>(beacon_template);
  beacon->status = status;
  beacon->chrome_error = attempt.result;
  if (!attempt.endpoint.address().empty()) {
    beacon->server_ip = attempt.endpoint.ToString();
  } else {
    beacon->server_ip = "";
  }
  return beacon;
}

}  // namespace

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    const std::string& upload_reporter_string,
    const DomainReliabilityContext::UploadAllowedCallback&
        upload_allowed_callback)
    : DomainReliabilityMonitor(url_request_context,
                               upload_reporter_string,
                               upload_allowed_callback,
                               std::make_unique<ActualTime>()) {}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    const std::string& upload_reporter_string,
    const DomainReliabilityContext::UploadAllowedCallback&
        upload_allowed_callback,
    std::unique_ptr<MockableTime> time)
    : time_(std::move(time)),
      dispatcher_(time_.get()),
      context_manager_(time_.get(),
                       upload_reporter_string,
                       upload_allowed_callback,
                       &dispatcher_),
      discard_uploads_set_(false) {
  DCHECK(url_request_context);
  uploader_ =
      DomainReliabilityUploader::Create(time_.get(), url_request_context);
  context_manager_.SetUploader(uploader_.get());
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void DomainReliabilityMonitor::Shutdown() {
  uploader_->Shutdown();
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
  for (size_t i = 0; kBakedInJsonConfigs[i]; ++i) {
    std::string_view json(kBakedInJsonConfigs[i]);
    std::unique_ptr<const DomainReliabilityConfig> config =
        DomainReliabilityConfig::FromJSON(json);
    // Guard against accidentally checking in malformed JSON configs.
    DCHECK(config->IsValid());
    context_manager_.AddContextForConfig(std::move(config));
  }
}

void DomainReliabilityMonitor::SetDiscardUploads(bool discard_uploads) {
  DCHECK(uploader_);

  uploader_->SetDiscardUploads(discard_uploads);
  discard_uploads_set_ = true;
}

void DomainReliabilityMonitor::OnBeforeRedirect(net::URLRequest* request) {
  DCHECK(discard_uploads_set_);

  // Record the redirect itself in addition to the final request. The fact that
  // a redirect is being followed indicates success.
  OnRequestLegComplete(RequestInfo(*request, net::OK));
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started,
                                           int net_error) {
  DCHECK(discard_uploads_set_);

  if (!started)
    return;
  RequestInfo request_info(*request, net_error);
  OnRequestLegComplete(request_info);

  if (request_info.response_info.network_accessed) {
    // A request was just using the network, so now is a good time to run any
    // pending and eligible uploads.
    dispatcher_.RunEligibleTasks();
  }
}

void DomainReliabilityMonitor::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  context_manager_.OnNetworkChanged(time_->NowTicks());
}

void DomainReliabilityMonitor::ClearBrowsingData(
    DomainReliabilityClearMode mode,
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter) {
  switch (mode) {
    case CLEAR_BEACONS:
      context_manager_.ClearBeacons(origin_filter);
      break;
    case CLEAR_CONTEXTS:
      context_manager_.RemoveContexts(origin_filter);
      break;
    case MAX_CLEAR_MODE:
      NOTREACHED_IN_MIGRATION();
  }
}

const DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    std::unique_ptr<const DomainReliabilityConfig> config) {
  DCHECK(config);
  return context_manager_.AddContextForConfig(std::move(config));
}

void DomainReliabilityMonitor::ForceUploadsForTesting() {
  dispatcher_.RunAllTasksForTesting();
}

void DomainReliabilityMonitor::OnRequestLegCompleteForTesting(
    const RequestInfo& request) {
  OnRequestLegComplete(request);
}

const DomainReliabilityContext*
DomainReliabilityMonitor::LookupContextForTesting(
    const std::string& hostname) const {
  return context_manager_.GetContext(hostname);
}

DomainReliabilityMonitor::RequestInfo::RequestInfo() = default;

DomainReliabilityMonitor::RequestInfo::RequestInfo(
    const net::URLRequest& request,
    int net_error)
    : url(request.url()),
      isolation_info(request.isolation_info()),
      net_error(net_error),
      response_info(request.response_info()),
      // This ignores cookie blocking by the NetworkDelegate, but probably
      // should not. Unclear if it's worth fixing.
      allow_credentials(request.allow_credentials()),
      connection_attempts(request.GetConnectionAttempts()),
      upload_depth(
          DomainReliabilityUploader::GetURLRequestUploadDepth(request)) {
  request.GetLoadTimingInfo(&load_timing_info);
  request.PopulateNetErrorDetails(&details);
  if (!request.GetTransactionRemoteEndpoint(&remote_endpoint))
    remote_endpoint = net::IPEndPoint();
}

DomainReliabilityMonitor::RequestInfo::RequestInfo(const RequestInfo& other) =
    default;

DomainReliabilityMonitor::RequestInfo::~RequestInfo() {}

// static
bool DomainReliabilityMonitor::RequestInfo::ShouldReportRequest(
    const DomainReliabilityMonitor::RequestInfo& request) {
  // Always report DR upload requests, even though they don't allow credentials.
  // Note: They are reported (i.e. generate a beacon) but do not necessarily
  // trigger an upload by themselves.
  if (request.upload_depth > 0)
    return true;

  // Don't report requests that weren't supposed to send credentials.
  if (!request.allow_credentials)
    return false;

  // Report requests that accessed the network or failed with an error code
  // that Domain Reliability is interested in.
  if (request.response_info.network_accessed)
    return true;
  if (request.net_error != net::OK)
    return true;
  if (request.details.quic_port_migration_detected)
    return true;

  return false;
}

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  // Check these again because unit tests call this directly.
  DCHECK(discard_uploads_set_);

  if (!RequestInfo::ShouldReportRequest(request))
    return;

  int response_code;
  if (request.response_info.headers) {
    response_code = request.response_info.headers->response_code();
  } else {
    response_code = -1;
  }

  net::ConnectionAttempt url_request_attempt(request.remote_endpoint,
                                             request.net_error);

  DomainReliabilityBeacon beacon_template;
  if (request.response_info.connection_info !=
      net::HttpConnectionInfo::kUNKNOWN) {
    beacon_template.protocol =
        GetDomainReliabilityProtocol(request.response_info.connection_info,
                                     request.response_info.ssl_info.is_valid());
  } else {
    // Use the connection info from the network error details if the response
    // is unavailable.
    beacon_template.protocol =
        GetDomainReliabilityProtocol(request.details.connection_info,
                                     request.response_info.ssl_info.is_valid());
  }
  GetDomainReliabilityBeaconQuicError(request.details.quic_connection_error,
                                      &beacon_template.quic_error);
  beacon_template.http_response_code = response_code;
  beacon_template.start_time = request.load_timing_info.request_start;
  beacon_template.elapsed = time_->NowTicks() - beacon_template.start_time;
  beacon_template.was_proxied = request.response_info.WasFetchedViaProxy();
  beacon_template.url = request.url;
  if (net::HttpCache::IsSplitCacheEnabled() &&
      !request.isolation_info.IsEmpty()) {
    // Set the IsolationInfo for the upload request to reflect that it isn't a
    // navigation, and since the requests will not be sent with credentials we
    // can use an empty `net::SiteForCookies()`.
    auto upload_isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        *request.isolation_info.top_frame_origin(),
        *request.isolation_info.frame_origin(), net::SiteForCookies());
    beacon_template.isolation_info = upload_isolation_info;
  }
  beacon_template.upload_depth = request.upload_depth;
  beacon_template.details = request.details;

  // This is not foolproof -- it's possible that we'll see the same error twice
  // (e.g. an SSL error during connection on one attempt, and then an error
  // that maps to the same code during a read).
  // TODO(juliatuttle): Find a way for this code to reliably tell whether we
  // eventually established a connection or not.
  bool url_request_attempt_is_duplicate = false;
  for (const auto& attempt : request.connection_attempts) {
    if (attempt.result == url_request_attempt.result)
      url_request_attempt_is_duplicate = true;

    std::unique_ptr<DomainReliabilityBeacon> beacon =
        CreateBeaconFromAttempt(beacon_template, attempt);
    if (beacon)
      context_manager_.RouteBeacon(std::move(beacon));
  }

  if (url_request_attempt_is_duplicate)
    return;

  std::unique_ptr<DomainReliabilityBeacon> beacon =
      CreateBeaconFromAttempt(beacon_template, url_request_attempt);
  if (beacon)
    context_manager_.RouteBeacon(std::move(beacon));
}

}  // namespace domain_reliability
