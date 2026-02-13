// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_request_impl.h"

#include <utility>

#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/zlib/google/compression_utils.h"

namespace syncer {

DeviceStatisticsRequestImpl::DeviceStatisticsRequestImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view user_agent,
    const CoreAccountInfo& account,
    const GURL& url)
    : account_(account),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      url_(url),
      user_agent_(user_agent) {
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
  CHECK(identity_manager_->HasAccountWithRefreshToken(account.account_id));
}

DeviceStatisticsRequestImpl::~DeviceStatisticsRequestImpl() = default;

void DeviceStatisticsRequestImpl::Start(base::OnceClosure callback) {
  CHECK(callback);
  CHECK_EQ(state_, State::kNotStarted);

  state_ = State::kInProgress;
  callback_ = std::move(callback);

  // Unretained() is safe because destroying `access_token_fetcher_` will also
  // cancel any in-flight operations.
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_.account_id,
      signin::OAuthConsumerId::kSyncDeviceStatisticsMetrics,
      base::BindOnce(&DeviceStatisticsRequestImpl::AccessTokenFetchComplete,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

DeviceStatisticsRequest::State DeviceStatisticsRequestImpl::GetState() const {
  return state_;
}

const std::vector<sync_pb::SyncEntity>&
DeviceStatisticsRequestImpl::GetResults() const {
  return results_;
}

void DeviceStatisticsRequestImpl::AccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    UpdateStateAndNotify(State::kFailed);

    // It is valid for the callback to delete `this`, so do not access any
    // members below here.
    return;
  }

  // Got an access token -- start the actual API request.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sync_device_statistics_metrics", R"(
        semantics {
          sender: "Chrome Sync"
          description:
            "Chrome sends this on profile startup, and periodically once per "
            "day, to collect metrics on all signed-in accounts."
          trigger:
            "Profile startup while already signed-in, and periodically once "
            "per day."
          data: "The user identifier."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "treib@chromium.org"
            }
            contacts {
              owners: "//components/sync/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2026-02-10"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable these requests by turning off 'Help improve "
            "Chrome's features and performance' in Chrome settings."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_info.token);
  resource_request->headers.SetHeader("Content-Encoding", "gzip");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      user_agent_);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  // Populate the GetUpdates message.
  sync_pb::ClientToServerMessage msg;
  msg.set_protocol_version(
      sync_pb::ClientToServerMessage::default_instance().protocol_version());
  *msg.mutable_bag_of_chips() = sync_pb::ChipBag();
  msg.mutable_client_status()->set_is_sync_feature_enabled(false);
  msg.set_api_key(google_apis::GetAPIKey());
  msg.set_share(account_.email);

  msg.set_message_contents(sync_pb::ClientToServerMessage_Contents_GET_UPDATES);
  sync_pb::GetUpdatesMessage& get_updates = *msg.mutable_get_updates();
  get_updates.set_fetch_folders(true);
  get_updates.set_need_encryption_key(false);
  get_updates.mutable_caller_info()->set_notifications_enabled(false);
  get_updates.set_get_updates_origin(
      sync_pb::SyncEnums::GetUpdatesOrigin::
          SyncEnums_GetUpdatesOrigin_DEVICE_STATISTICS_METRICS);
  sync_pb::DataTypeProgressMarker& progress =
      *get_updates.add_from_progress_marker();
  progress.set_data_type_id(
      GetSpecificsFieldNumberFromDataType(syncer::DEVICE_INFO));
  progress.mutable_get_update_triggers()->set_initial_sync_in_progress(true);

  // Serialize and gzip the message.
  std::string serialized_msg;
  msg.SerializeToString(&serialized_msg);
  std::string request_to_send;
  compression::GzipCompress(serialized_msg, &request_to_send);

  simple_url_loader_->AttachStringForUpload(request_to_send,
                                            "application/octet-stream");

  simple_url_loader_->SetAllowHttpErrorResults(true);

  // Unretained() is safe because destroying `simple_url_loader_` will cancel
  // any in-flight operations.
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceStatisticsRequestImpl::SimpleLoaderComplete,
                     base::Unretained(this), access_token_info));
}

void DeviceStatisticsRequestImpl::SimpleLoaderComplete(
    signin::AccessTokenInfo access_token_info,
    std::optional<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  simple_url_loader_.reset();

  // If the response code indicates that the token might not be valid,
  // invalidate the token and try again.
  if (response_code == net::HTTP_UNAUTHORIZED && !has_retried_authorization_) {
    identity_manager_->RemoveAccessTokenFromCache(
        account_.account_id, signin::OAuthConsumerId::kWebHistoryService,
        access_token_info.token);
    has_retried_authorization_ = true;
    state_ = State::kNotStarted;  // Avoid CHECK() in Start().
    Start(std::move(callback_));
    return;
  }

  if (response_code != net::HTTP_OK) {
    UpdateStateAndNotify(State::kFailed);
    return;
  }

  if (!response_body) {
    UpdateStateAndNotify(State::kFailed);
    return;
  }

  sync_pb::ClientToServerResponse response;
  if (!response.ParseFromString(response_body.value())) {
    UpdateStateAndNotify(State::kFailed);
    return;
  }

  for (const sync_pb::SyncEntity& entity : response.get_updates().entries()) {
    results_.push_back(entity);
  }

  UpdateStateAndNotify(State::kComplete);
  // It is valid for the callback to delete `this`, so do not access any
  // members below here.
}

void DeviceStatisticsRequestImpl::UpdateStateAndNotify(State state) {
  CHECK(state == State::kComplete || state == State::kFailed);
  CHECK(callback_);
  state_ = state;
  std::move(callback_).Run();
}

}  // namespace syncer
