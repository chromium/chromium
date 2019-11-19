// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/google_configs.h"
#include "components/domain_reliability/header.h"
#include "components/domain_reliability/quic_error_mapping.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

namespace {

int URLRequestStatusToNetError(const net::URLRequestStatus& status) {
  switch (status.status()) {
    case net::URLRequestStatus::SUCCESS:
      return net::OK;
    case net::URLRequestStatus::IO_PENDING:
      return net::ERR_IO_PENDING;
    case net::URLRequestStatus::CANCELED:
      return net::ERR_ABORTED;
    case net::URLRequestStatus::FAILED:
      return status.error();
    default:
      NOTREACHED();
      return net::ERR_UNEXPECTED;
  }
}

// Creates a new beacon based on |beacon_template| but fills in the status,
// chrome_error, and server_ip fields based on the endpoint and result of
// |attempt|.
//
// If there is no matching status for the result, returns false (which
// means the attempt should not result in a beacon being reported).
std::unique_ptr<DomainReliabilityBeacon> CreateBeaconFromAttempt(
    const DomainReliabilityBeacon& beacon_template,
    const net::ConnectionAttempt& attempt) {
  std::string status;
  if (!GetDomainReliabilityBeaconStatus(
          attempt.result, beacon_template.http_response_code, &status)) {
    return std::unique_ptr<DomainReliabilityBeacon>();
  }

  std::unique_ptr<DomainReliabilityBeacon> beacon(
      new DomainReliabilityBeacon(beacon_template));
  beacon->status = status;
  beacon->chrome_error = attempt.result;
  if (!attempt.endpoint.address().empty())
    beacon->server_ip = attempt.endpoint.ToString();
  else
    beacon->server_ip = "";
  return beacon;
}

const char* kDomainReliabilityHeaderName = "NEL";

}  // namespace

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string,
    const DomainReliabilityContext::UploadAllowedCallback&
        upload_allowed_callback)
    : time_(new ActualTime()),
      upload_reporter_string_(upload_reporter_string),
      upload_allowed_callback_(upload_allowed_callback),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      context_manager_(this),
      discard_uploads_set_(false) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    const std::string& upload_reporter_string,
    const DomainReliabilityContext::UploadAllowedCallback&
        upload_allowed_callback,
    std::unique_ptr<MockableTime> time)
    : time_(std::move(time)),
      upload_reporter_string_(upload_reporter_string),
      upload_allowed_callback_(upload_allowed_callback),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      context_manager_(this),
      discard_uploads_set_(false) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void DomainReliabilityMonitor::InitURLRequestContext(
    net::URLRequestContext* url_request_context) {
  DCHECK(url_request_context);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      new net::TrivialURLRequestContextGetter(
          url_request_context, base::ThreadTaskRunnerHandle::Get());
  InitURLRequestContext(url_request_context_getter);
}

void DomainReliabilityMonitor::InitURLRequestContext(
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  uploader_ = DomainReliabilityUploader::Create(time_.get(),
                                                url_request_context_getter);
}

void DomainReliabilityMonitor::Shutdown() {
  uploader_->Shutdown();
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
  for (size_t i = 0; kBakedInJsonConfigs[i]; ++i) {
    base::StringPiece json(kBakedInJsonConfigs[i]);
    std::unique_ptr<const DomainReliabilityConfig> config =
        DomainReliabilityConfig::FromJSON(json);
    if (!config) {
      DLOG(WARNING) << "Baked-in Domain Reliability config failed to parse: "
                    << json;
      continue;
    }
    context_manager_.AddContextForConfig(std::move(config));
  }

  std::vector<std::unique_ptr<DomainReliabilityConfig>> google_configs;
  GetAllGoogleConfigs(&google_configs);
  for (auto& google_config : google_configs)
    context_manager_.AddContextForConfig(std::move(google_config));
}

void DomainReliabilityMonitor::SetDiscardUploads(bool discard_uploads) {
  DCHECK(uploader_);

  uploader_->SetDiscardUploads(discard_uploads);
  discard_uploads_set_ = true;
}

void DomainReliabilityMonitor::OnBeforeRedirect(net::URLRequest* request) {
  DCHECK(discard_uploads_set_);

  // Record the redirect itself in addition to the final request.
  OnRequestLegComplete(RequestInfo(*request));
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started) {
  DCHECK(discard_uploads_set_);

  if (!started)
    return;
  RequestInfo request_info(*request);
  OnRequestLegComplete(request_info);

  if (request_info.response_info.network_accessed) {
    // A request was just using the network, so now is a good time to run any
    // pending and eligible uploads.
    dispatcher_.RunEligibleTasks();
  }
}

void DomainReliabilityMonitor::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  last_network_change_time_ = time_->NowTicks();
}

void DomainReliabilityMonitor::ClearBrowsingData(
    DomainReliabilityClearMode mode,
    const base::Callback<bool(const GURL&)>& origin_filter) {
  switch (mode) {
    case CLEAR_BEACONS:
      context_manager_.ClearBeacons(origin_filter);
      break;
    case CLEAR_CONTEXTS:
      context_manager_.RemoveContexts(origin_filter);
      break;
    case MAX_CLEAR_MODE:
      NOTREACHED();
  }
}

std::unique_ptr<base::Value> DomainReliabilityMonitor::GetWebUIData() const {
  std::unique_ptr<base::DictionaryValue> data_value(
      new base::DictionaryValue());
  data_value->Set("contexts", context_manager_.GetWebUIData());
  return std::move(data_value);
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    std::unique_ptr<const DomainReliabilityConfig> config) {
  return context_manager_.AddContextForConfig(std::move(config));
}

void DomainReliabilityMonitor::ForceUploadsForTesting() {
  dispatcher_.RunAllTasksForTesting();
}

std::unique_ptr<DomainReliabilityContext>
DomainReliabilityMonitor::CreateContextForConfig(
    std::unique_ptr<const DomainReliabilityConfig> config) {
  DCHECK(config);
  DCHECK(config->IsValid());

  return std::make_unique<DomainReliabilityContext>(
      time_.get(), scheduler_params_, upload_reporter_string_,
      &last_network_change_time_, upload_allowed_callback_, &dispatcher_,
      uploader_.get(), std::move(config));
}

DomainReliabilityMonitor::RequestInfo::RequestInfo() {}

DomainReliabilityMonitor::RequestInfo::RequestInfo(
    const net::URLRequest& request)
    : url(request.url()),
      status(request.status()),
      response_info(request.response_info()),
      load_flags(request.load_flags()),
      upload_depth(
          DomainReliabilityUploader::GetURLRequestUploadDepth(request)) {
  request.GetLoadTimingInfo(&load_timing_info);
  request.GetConnectionAttempts(&connection_attempts);
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
  // Always report upload requests, even though they have DO_NOT_SEND_COOKIES.
  if (request.upload_depth > 0)
    return true;

  // Don't report requests that weren't supposed to send cookies.
  if (request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES)
    return false;

  // Report requests that accessed the network or failed with an error code
  // that Domain Reliability is interested in.
  if (request.response_info.network_accessed)
    return true;
  if (URLRequestStatusToNetError(request.status) != net::OK)
    return true;
  if (request.details.quic_port_migration_detected)
    return true;

  return false;
}

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  // Check these again because unit tests call this directly.
  DCHECK(discard_uploads_set_);

  MaybeHandleHeader(request);

  if (!RequestInfo::ShouldReportRequest(request))
    return;

  int response_code;
  if (request.response_info.headers)
    response_code = request.response_info.headers->response_code();
  else
    response_code = -1;

  net::ConnectionAttempt url_request_attempt(
      request.remote_endpoint, URLRequestStatusToNetError(request.status));

  DomainReliabilityBeacon beacon_template;
  if (request.response_info.connection_info !=
      net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN) {
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
  beacon_template.was_proxied = request.response_info.was_fetched_via_proxy;
  beacon_template.url = request.url;
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

void DomainReliabilityMonitor::MaybeHandleHeader(
    const RequestInfo& request) {
  if (!request.response_info.headers)
    return;

  size_t iter = 0;
  std::string kHeaderNameString(kDomainReliabilityHeaderName);

  std::string header_value;
  if (!request.response_info.headers->EnumerateHeader(
          &iter, kHeaderNameString, &header_value)) {
    // No header found.
    return;
  }

  std::string ignored_header_value;
  if (request.response_info.headers->EnumerateHeader(
          &iter, kHeaderNameString, &ignored_header_value)) {
    DLOG(WARNING) << "Request to " << request.url << " had (at least) two "
                  << kHeaderNameString << " headers: \"" << header_value
                  << "\" and \"" << ignored_header_value << "\".";
    return;
  }

  std::unique_ptr<DomainReliabilityHeader> parsed =
      DomainReliabilityHeader::Parse(header_value);
  GURL origin = request.url.GetOrigin();
  switch (parsed->status()) {
    case DomainReliabilityHeader::PARSE_SET_CONFIG:
      {
        base::TimeDelta max_age = parsed->max_age();
        context_manager_.SetConfig(origin, parsed->ReleaseConfig(), max_age);
      }
      break;
    case DomainReliabilityHeader::PARSE_CLEAR_CONFIG:
      context_manager_.ClearConfig(origin);
      break;
    case DomainReliabilityHeader::PARSE_ERROR:
      DLOG(WARNING) << "Request to " << request.url << " had invalid "
                    << kHeaderNameString << " header \"" << header_value
                    << "\".";
      break;
  }
}

base::WeakPtr<DomainReliabilityMonitor>
DomainReliabilityMonitor::MakeWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace domain_reliability
