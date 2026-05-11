// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_update_protocol_manager.h"

#include <utility>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

// V5UpdateProtocolManager implementation --------------------------------

V5UpdateProtocolManager::V5UpdateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    // TODO(crbug.com/362791941): remove v4 references
    const V4ProtocolConfig& config,
    V5UpdateCallback update_callback)
    : SBUpdateProtocolManager(std::move(url_loader_factory), config),
      update_callback_(update_callback) {
  // Do not auto-schedule updates. Let the owner (V4LocalDatabaseManager) do it
  // when it is ready to process updates.
}

V5UpdateProtocolManager::~V5UpdateProtocolManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool V5UpdateProtocolManager::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

void V5UpdateProtocolManager::ScheduleNextUpdate(
    std::unique_ptr<StoreStateMap> store_state_map) {
  // Convert input to format closer to what the v5 request will expect.
  std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping;
  CHECK(!store_state_map->empty());
  for (auto& [list_id, client_version] : *store_state_map) {
    list_identifier_to_version_mapping.push_back(
        ListIdentifierAndVersion(list_id, std::move(client_version)));
  }
  ScheduleNextUpdateInternal(/*back_off=*/false,
                             std::move(list_identifier_to_version_mapping));
}

void V5UpdateProtocolManager::ScheduleNextUpdateInternal(
    bool back_off,
    std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config_.disable_auto_update) {
    CHECK(!IsUpdateScheduled());
    return;
  }

  // Schedule the new update.
  base::TimeDelta next_update_interval = GetNextUpdateInterval(back_off);
  ScheduleNextUpdateAfterInterval(
      next_update_interval, std::move(list_identifier_to_version_mapping));
}

void V5UpdateProtocolManager::ScheduleNextUpdateAfterInterval(
    base::TimeDelta interval,
    std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(interval >= base::TimeDelta());

  next_update_time_ = base::Time::Now() + interval;
  // Unschedule any current timer.
  update_timer_.Stop();
  update_timer_.Start(
      FROM_HERE, interval,
      base::BindOnce(&V5UpdateProtocolManager::IssueUpdateRequest,
                     weak_factory_.GetWeakPtr(),
                     std::move(list_identifier_to_version_mapping)));
}

void V5UpdateProtocolManager::IssueUpdateRequest(
    std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There must not already be an update request pending.
  CHECK(!request_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_v5_update", R"(
       semantics {
          sender: "Safe Browsing"
          description:
            "Safe Browsing periodically issues a request to Google to get the "
            "latest database of blocklisted or allowlisted URL hashes."
          trigger:
            "On a timer, generally multiple times each hour."
          data:
             "The state of the local DB is sent so the server can send just "
             "the changes. This doesn't include any user data."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "thefrog@chromium.org"
            }
            contacts {
              email: "chrome-counter-abuse-alerts@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2026-04-24"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable Safe Browsing by checking 'No protection' in "
            "Chromium settings under Security > Safe Browsing. The feature is "
            "enabled by default."
          chrome_policy {
            SafeBrowsingProtectionLevel {
              policy_options {mode: MANDATORY}
              SafeBrowsingProtectionLevel: 0
            }
          }
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
          deprecated_policies: "SafeBrowsingEnabled"
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string request_base64 =
      GetBase64SerializedUpdateRequestProto(list_identifier_to_version_mapping);
  std::string url = base::StringPrintf(
      "https://safebrowsing.googleapis.com/v5/hashLists:batchGet"
      "?$req=%s&$ct=application/x-protobuf",
      request_base64.c_str());
  auto api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        base::EscapeQueryParamValue(api_key, true).c_str());
  }
  resource_request->url = GURL(url);
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // The v5 User-Agent header docs state: "While there is no prescribed format
  // for supplying the client identification in this header, we suggest simply
  // including the original client ID and client version separated by a space
  // character or a slash character."
  // Thus, this uses the same `client_name` and `version` originally used for
  // v4, here separated with a space character.
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kUserAgent,
      base::StrCat({config_.client_name, " ", config_.version}));

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  loader->SetTimeoutDuration(base::Seconds(kTimerUpdateWaitSecMax));
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&V5UpdateProtocolManager::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(),
                     std::move(list_identifier_to_version_mapping)));

  request_ = std::move(loader);
}

// static
std::string V5UpdateProtocolManager::GetBase64SerializedUpdateRequestProto(
    const std::vector<ListIdentifierAndVersion>&
        list_identifier_to_version_mapping) {
  // Build the request.
  V5::BatchGetHashListsRequest request;
  for (const auto& entry : list_identifier_to_version_mapping) {
    const auto& list_name = GetV5ListName(entry.list_identifier);
    const auto& list_version = entry.list_version;
    request.add_names(list_name);
    if (!list_version.empty()) {
      request.add_version(list_version);
    }
  }

  // Serialize and Base64 encode.
  std::string request_data, request_base64;
  request.SerializeToString(&request_data);
  base::Base64UrlEncode(request_data,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &request_base64);
  return request_base64;
}

void V5UpdateProtocolManager::OnURLLoaderComplete(
    std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping,
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int response_code = 0;
  if (request_->ResponseInfo() && request_->ResponseInfo()->headers) {
    response_code = request_->ResponseInfo()->headers->response_code();
  }

  ParsedResponse result = OnURLLoaderCompleteInternal(
      request_->NetError(), response_code, std::move(response_body),
      list_identifier_to_version_mapping);

  request_.reset();
  // Set the next update interval based on the minimum wait duration
  // across lists.
  next_update_interval_ = result.minimum_wait_duration;
  // The caller should update its state now based on the parsed response.
  // The callback must call `ScheduleNextUpdate()` at the end to resume
  // downloading updates.
  update_callback_.Run(std::move(result.hash_list_map));
}

V5UpdateProtocolManager::ParsedResponse
V5UpdateProtocolManager::OnURLLoaderCompleteInternal(
    int net_error,
    int response_code,
    const std::optional<std::string>& response_body,
    const std::vector<ListIdentifierAndVersion>&
        list_identifier_to_version_mapping) {
  // Used for chrome://safe-browsing debugging page.
  last_response_code_ = response_code;
  last_response_time_ = base::Time::Now();

  if (net_error == net::OK && response_code == net::HTTP_OK) {
    ResetUpdateErrors();
    ParsedResponse parsed_response =
        ParseUpdateResponse(response_body, list_identifier_to_version_mapping);
    return parsed_response;
  }

  // TODO(crbug.com/362791941): failed responses
  ParsedResponse failed_response;
  return failed_response;
}

V5UpdateProtocolManager::ParsedResponse
V5UpdateProtocolManager::ParseUpdateResponse(
    const std::optional<std::string>& response_body,
    const std::vector<ListIdentifierAndVersion>&
        list_identifier_to_version_mapping) {
  V5::BatchGetHashListsResponse server_response;
  server_response.ParseFromString(response_body.value());
  // TODO(crbug.com/362791941): failed parsing - including confirming response #
  // lists matches request # lists
  std::map<ListIdentifier, V5::HashList> parsed_response;
  // TODO(crbug.com/362791941): consider non-optional + initialize to max time
  std::optional<base::TimeDelta> overall_minimum_wait_duration;
  for (int i = 0; i < server_response.hash_lists_size(); ++i) {
    V5::HashList& hash_list = *server_response.mutable_hash_lists(i);
    // The v5 API sends back the lists in the same order as requested.
    ListIdentifier list_identifier =
        list_identifier_to_version_mapping[i].list_identifier;

    // Save off the smallest minimum_wait_duration across lists and use that to
    // determine when next to trigger an update request.
    std::optional<base::TimeDelta> list_minimum_wait_duration;
    if (hash_list.has_minimum_wait_duration()) {
      // TODO(crbug.com/362791941): fail if invalid duration (negative)
      const auto& duration = hash_list.minimum_wait_duration();
      list_minimum_wait_duration = base::Seconds(duration.seconds()) +
                                   base::Nanoseconds(duration.nanos());
    } else {
      // If minimum_wait_duration is unset (or 0, handled above), clients are
      // expected to fetch immediately. Note that this is not expected to happen
      // in our case because we are not setting any client-specified constraints
      // that would trigger an unset (or 0) minimum_wait_duration. But, we
      // respect the server's response if it responds this way regardless.
      list_minimum_wait_duration = base::TimeDelta();
    }
    if (!overall_minimum_wait_duration.has_value() ||
        list_minimum_wait_duration.value() <
            overall_minimum_wait_duration.value()) {
      overall_minimum_wait_duration = list_minimum_wait_duration.value();
    }

    V5::HashList list_to_insert;
    list_to_insert.Swap(&hash_list);
    parsed_response[list_identifier] = std::move(list_to_insert);
  }

  return ParsedResponse(std::move(parsed_response),
                        overall_minimum_wait_duration.value());
}

V5UpdateProtocolManager::ListIdentifierAndVersion::ListIdentifierAndVersion(
    ListIdentifier list_identifier,
    std::string list_version)
    : list_identifier(list_identifier), list_version(std::move(list_version)) {}

V5UpdateProtocolManager::ParsedResponse::ParsedResponse() = default;

V5UpdateProtocolManager::ParsedResponse::ParsedResponse(
    std::map<ListIdentifier, V5::HashList> hash_list_map,
    base::TimeDelta minimum_wait_duration)
    : hash_list_map(std::move(hash_list_map)),
      minimum_wait_duration(minimum_wait_duration) {}

V5UpdateProtocolManager::ParsedResponse::~ParsedResponse() = default;
V5UpdateProtocolManager::ParsedResponse::ParsedResponse(ParsedResponse&&) =
    default;
V5UpdateProtocolManager::ParsedResponse&
V5UpdateProtocolManager::ParsedResponse::operator=(ParsedResponse&&) = default;

}  // namespace safe_browsing
