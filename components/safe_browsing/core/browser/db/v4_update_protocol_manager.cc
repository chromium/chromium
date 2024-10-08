// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_update_protocol_manager.h"

#include <utility>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using base::Time;
using enum safe_browsing::ExtendedReportingLevel;

namespace {

// Enumerate parsing failures for histogramming purposes.  DO NOT CHANGE
// THE ORDERING OF THESE VALUES.
enum ParseResultType {
  // Error parsing the protocol buffer from a string.
  PARSE_FROM_STRING_ERROR = 0,

  // No platform_type set in the response.
  NO_PLATFORM_TYPE_ERROR = 1,

  // No threat_entry_type set in the response.
  NO_THREAT_ENTRY_TYPE_ERROR = 2,

  // No threat_type set in the response.
  NO_THREAT_TYPE_ERROR = 3,

  // No state set in the response for one or more lists.
  NO_STATE_ERROR = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_RESULT_TYPE_MAX = 5
};

// Record parsing errors of an update result.
void RecordParseUpdateResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4Update.Parse.Result", result_type,
                            PARSE_RESULT_TYPE_MAX);
}

void RecordUpdateResult(safe_browsing::V4OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4Update.Result", result,
      safe_browsing::V4OperationResult::OPERATION_RESULT_MAX);
}

}  // namespace

namespace safe_browsing {

// Minimum time, in seconds, from start up before we must issue an update query.
static const int kV4TimerStartIntervalSecMin = 60;

// Maximum time, in seconds, from start up before we must issue an update query.
static const int kV4TimerStartIntervalSecMax = 300;

// Maximum time, in seconds, to wait for a response to an update request.
static const int kV4TimerUpdateWaitSecMax = 15 * 60;  // 15 minutes

ChromeClientInfo::SafeBrowsingReportingPopulation GetReportingLevelProtoValue(
    ExtendedReportingLevel reporting_level) {
  switch (reporting_level) {
    case SBER_LEVEL_OFF:
      return ChromeClientInfo::OPT_OUT;
    case SBER_LEVEL_LEGACY:
      return ChromeClientInfo::EXTENDED;
    case SBER_LEVEL_SCOUT:
      return ChromeClientInfo::SCOUT;
    case SBER_LEVEL_ENHANCED_PROTECTION:
      return ChromeClientInfo::ENHANCED_PROTECTION;
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected reporting_level!";
      return ChromeClientInfo::UNSPECIFIED;
  }
}

// V4UpdateProtocolManager implementation --------------------------------

void V4UpdateProtocolManager::ResetUpdateErrors() {
  update_error_count_ = 0;
  update_back_off_mult_ = 1;
}

V4UpdateProtocolManager::V4UpdateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config,
    V4UpdateCallback update_callback,
    ExtendedReportingLevelCallback extended_reporting_level_callback)
    : update_error_count_(0),
      update_back_off_mult_(1),
      next_update_interval_(
          base::Seconds(base::RandInt(kV4TimerStartIntervalSecMin,
                                      kV4TimerStartIntervalSecMax))),
      config_(config),
      url_loader_factory_(url_loader_factory),
      update_callback_(update_callback),
      extended_reporting_level_callback_(extended_reporting_level_callback) {
  // Do not auto-schedule updates. Let the owner (V4LocalDatabaseManager) do it
  // when it is ready to process updates.
}

V4UpdateProtocolManager::~V4UpdateProtocolManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool V4UpdateProtocolManager::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

void V4UpdateProtocolManager::ScheduleNextUpdate(
    std::unique_ptr<StoreStateMap> store_state_map) {
  store_state_map_ = std::move(store_state_map);
  ScheduleNextUpdateWithBackoff(false);
}

void V4UpdateProtocolManager::ScheduleNextUpdateWithBackoff(bool back_off) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config_.disable_auto_update) {
    DCHECK(!IsUpdateScheduled());
    return;
  }

  // Reschedule with the new update.
  base::TimeDelta next_update_interval = GetNextUpdateInterval(back_off);
  ScheduleNextUpdateAfterInterval(next_update_interval);
}

// According to section 5 of the SafeBrowsing protocol specification, we must
// back off after a certain number of errors.
base::TimeDelta V4UpdateProtocolManager::GetNextUpdateInterval(bool back_off) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(next_update_interval_.is_positive());

  base::TimeDelta next = next_update_interval_;
  if (back_off) {
    next = V4ProtocolManagerUtil::GetNextBackOffInterval(
        &update_error_count_, &update_back_off_mult_);
  }

  if (!last_response_time_.is_null()) {
    // The callback spent some time updating the database, including disk I/O.
    // Do not wait that extra time.
    base::TimeDelta callback_time = Time::Now() - last_response_time_;
    if (callback_time < next) {
      next -= callback_time;
    } else {
      // If the callback took too long, schedule the next update with no delay.
      next = base::TimeDelta();
    }
  }
  DVLOG(1) << "V4UpdateProtocolManager::GetNextUpdateInterval: "
           << "next_interval: " << next;
  return next;
}

void V4UpdateProtocolManager::ScheduleNextUpdateAfterInterval(
    base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interval >= base::TimeDelta());

  next_update_time_ = Time::Now() + interval;
  // Unschedule any current timer.
  update_timer_.Stop();
  update_timer_.Start(FROM_HERE, interval, this,
                      &V4UpdateProtocolManager::IssueUpdateRequest);
}

std::string V4UpdateProtocolManager::GetBase64SerializedUpdateRequestProto() {
  DCHECK(!store_state_map_->empty());
  // Build the request. Client info and client states are not added to the
  // request protocol buffer. Client info is passed as params in the url.
  FetchThreatListUpdatesRequest request;
  for (const auto& entry : *store_state_map_) {
    const auto& list_to_update = entry.first;
    const auto& state = entry.second;
    ListUpdateRequest* list_update_request = request.add_list_update_requests();
    list_update_request->set_platform_type(list_to_update.platform_type());
    list_update_request->set_threat_entry_type(
        list_to_update.threat_entry_type());
    list_update_request->set_threat_type(list_to_update.threat_type());

    if (!state.empty()) {
      list_update_request->set_state(state);
    }

    list_update_request->mutable_constraints()->add_supported_compressions(RAW);
    list_update_request->mutable_constraints()->add_supported_compressions(
        RICE);
  }

  if (!extended_reporting_level_callback_.is_null()) {
    request.mutable_chrome_client_info()->set_reporting_population(
        GetReportingLevelProtoValue(extended_reporting_level_callback_.Run()));
  }

  V4ProtocolManagerUtil::SetClientInfoFromConfig(request.mutable_client(),
                                                 config_);

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  request.SerializeToString(&req_data);
  base::Base64UrlEncode(req_data, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &req_base64);
  return req_base64;
}

bool V4UpdateProtocolManager::ParseUpdateResponse(
    const std::string& data,
    ParsedServerResponse* parsed_server_response) {
  FetchThreatListUpdatesResponse response;

  if (!response.ParseFromString(data)) {
    RecordParseUpdateResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    int64_t minimum_wait_duration_seconds =
        response.minimum_wait_duration().seconds();

    // Do not let the next_update_interval_ to be too low.
    if (minimum_wait_duration_seconds < kV4TimerStartIntervalSecMin) {
      minimum_wait_duration_seconds = kV4TimerStartIntervalSecMin;
    }
    next_update_interval_ = base::Seconds(minimum_wait_duration_seconds);
  }

  for (ListUpdateResponse& list_update_response :
       *response.mutable_list_update_responses()) {
    if (!list_update_response.has_platform_type()) {
      RecordParseUpdateResult(NO_PLATFORM_TYPE_ERROR);
    } else if (!list_update_response.has_threat_entry_type()) {
      RecordParseUpdateResult(NO_THREAT_ENTRY_TYPE_ERROR);
    } else if (!list_update_response.has_threat_type()) {
      RecordParseUpdateResult(NO_THREAT_TYPE_ERROR);
    } else if (!list_update_response.has_new_client_state()) {
      RecordParseUpdateResult(NO_STATE_ERROR);
    } else {
      std::unique_ptr<ListUpdateResponse> add(new ListUpdateResponse);
      add->Swap(&list_update_response);
      parsed_server_response->push_back(std::move(add));
    }
  }
  return true;
}

void V4UpdateProtocolManager::IssueUpdateRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If an update request is already pending, record and return silently.
  if (request_) {
    RecordUpdateResult(V4OperationResult::ALREADY_PENDING_ERROR);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_v4_update", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "Safe Browsing issues a request to Google every 30 minutes or so "
            "to get the latest database of hashes of bad URLs."
          trigger:
            "On a timer, approximately every 30 minutes."
          data:
             "The state of the local DB is sent so the server can send just "
             "the changes. This doesn't include any user data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing by unchecking 'Protect you and "
            "your device from dangerous sites' in Chromium settings under "
            "Privacy. The feature is enabled by default."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string req_base64 = GetBase64SerializedUpdateRequestProto();
  GetUpdateUrlAndHeaders(req_base64, &resource_request->url,
                         &resource_request->headers);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&V4UpdateProtocolManager::OnURLLoaderComplete,
                     base::Unretained(this)));

  request_ = std::move(loader);

  // Begin the update request timeout.
  timeout_timer_.Start(FROM_HERE, base::Seconds(kV4TimerUpdateWaitSecMax), this,
                       &V4UpdateProtocolManager::HandleTimeout);
}

void V4UpdateProtocolManager::HandleTimeout() {
  request_.reset();
  ScheduleNextUpdateWithBackoff(true);
}

// SafeBrowsing request responses are handled here.
void V4UpdateProtocolManager::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int response_code = 0;
  if (request_->ResponseInfo() && request_->ResponseInfo()->headers)
    response_code = request_->ResponseInfo()->headers->response_code();

  std::string data;
  if (response_body)
    data = *response_body;

  OnURLLoaderCompleteInternal(request_->NetError(), response_code, data);
}

void V4UpdateProtocolManager::OnURLLoaderCompleteInternal(
    int net_error,
    int response_code,
    const std::string& data) {
  timeout_timer_.Stop();

  last_response_code_ = response_code;
  RecordHttpResponseOrErrorCode("SafeBrowsing.V4Update.Network.Result",
                                net_error, last_response_code_);

  last_response_time_ = Time::Now();

  std::unique_ptr<ParsedServerResponse> parsed_server_response(
      new ParsedServerResponse);
  if (net_error == net::OK && last_response_code_ == net::HTTP_OK) {
    RecordUpdateResult(V4OperationResult::STATUS_200);
    ResetUpdateErrors();
    if (!ParseUpdateResponse(data, parsed_server_response.get())) {
      parsed_server_response->clear();
      RecordUpdateResult(V4OperationResult::PARSE_ERROR);
    }
    request_.reset();

    UMA_HISTOGRAM_COUNTS_1M("SafeBrowsing.V4Update.ResponseSizeKB",
                            data.size() / 1024);

    // The caller should update its state now, based on parsed_server_response.
    // The callback must call ScheduleNextUpdate() at the end to resume
    // downloading updates.
    update_callback_.Run(std::move(parsed_server_response));
  } else {
    DVLOG(1) << "SafeBrowsing GetEncodedUpdates request for: "
             << request_->GetFinalURL() << " failed with error: " << net_error
             << " and response code: " << last_response_code_;

    if (net_error != net::OK) {
      RecordUpdateResult(V4OperationResult::NETWORK_ERROR);
    } else {
      RecordUpdateResult(V4OperationResult::HTTP_ERROR);
    }
    // TODO(vakh): Figure out whether it is just a network error vs backoff vs
    // another condition and RecordUpdateResult more accurately.

    request_.reset();
    ScheduleNextUpdateWithBackoff(true);
  }
}

void V4UpdateProtocolManager::GetUpdateUrlAndHeaders(
    const std::string& req_base64,
    GURL* gurl,
    net::HttpRequestHeaders* headers) const {
  V4ProtocolManagerUtil::GetRequestUrlAndHeaders(
      req_base64, "threatListUpdates:fetch", config_, gurl, headers);
}

void V4UpdateProtocolManager::CollectUpdateInfo(
    DatabaseManagerInfo::UpdateInfo* update_info) {
  if (last_response_code_)
    update_info->set_network_status_code(last_response_code_);

  if (last_response_time_.InMillisecondsSinceUnixEpoch()) {
    update_info->set_last_update_time_millis(
        last_response_time_.InMillisecondsSinceUnixEpoch());
  }

  if (next_update_time_) {
    update_info->set_next_update_time_millis(
        next_update_time_.value().InMillisecondsSinceUnixEpoch());
  }
}

const base::Time& V4UpdateProtocolManager::last_response_time() const {
  return last_response_time_;
}

}  // namespace safe_browsing
