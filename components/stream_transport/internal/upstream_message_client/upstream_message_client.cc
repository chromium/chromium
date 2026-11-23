// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/upstream_message_client/upstream_message_client.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/stream_transport/internal/features.h"
#include "components/stream_transport/internal/metrics_utils.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/is_potentially_trustworthy.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace stream_transport {

namespace {

browser_actuator::ActuatorUpstreamPayloadType ToProtoPayloadType(
    PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kUnspecified:
      CHECK(false);
    case PayloadType::kControl:
      return browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND;
    case PayloadType::kExperimentalTriggering:
      return browser_actuator::
          ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING;
    case PayloadType::kActuation:
      return browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_ACTUATION;
    case PayloadType::kTaskIngress:
      return browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_TASK_INGRESS;
    case PayloadType::kProbe:
      return browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE;
  }
  NOTREACHED();
}

}  // namespace

struct UpstreamMessageClient::PendingRequest {
  PendingRequest(std::string serialized_request,
                 UpstreamRequestLog metrics_log,
                 SendCompleteCallback callback)
      : serialized_request(std::move(serialized_request)),
        metrics_log(std::move(metrics_log)),
        callback(std::move(callback)) {}
  ~PendingRequest() = default;

  std::string serialized_request;
  UpstreamRequestLog metrics_log;
  SendCompleteCallback callback;
  CoreAccountId access_token_account_id;
  std::string access_token;
  bool retried_auth_failure = false;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher;
  std::unique_ptr<network::SimpleURLLoader> url_loader;
};

UpstreamMessageClient::UpstreamMessageClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    GURL endpoint,
    net::NetworkTrafficAnnotationTag traffic_annotation)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      endpoint_(std::move(endpoint)),
      traffic_annotation_(traffic_annotation) {
  CHECK(identity_manager_);
}

UpstreamMessageClient::~UpstreamMessageClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpstreamMessageClient::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_requests_.clear();
}

size_t UpstreamMessageClient::GetPendingRequestsCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_requests_.size();
}

browser_actuator::ActuatorUpstreamMessage
UpstreamMessageClient::SendUpstreamMessage(
    std::string_view session_id,
    int64_t client_sequence_number,
    std::optional<int64_t> responding_to_sequence_number,
    PayloadType payload_type,
    const google::protobuf::MessageLite& message,
    SendCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const browser_actuator::ActuatorUpstreamPayloadType proto_payload_type =
      ToProtoPayloadType(payload_type);

  browser_actuator::ActuatorUpstreamMessage upstream_message;
  upstream_message.set_session_id(session_id);
  upstream_message.set_client_sequence_number(client_sequence_number);
  if (responding_to_sequence_number.has_value()) {
    upstream_message.set_responding_to_sequence_number(
        *responding_to_sequence_number);
  }
  // TODO(b/560670671): Also populate `supported_features` and consider sending
  // `capabilities` only on the session-opening envelope. Populates the Chrome
  // version fields on every envelope for host capability gating.
  auto* capabilities = upstream_message.mutable_capabilities();
  capabilities->set_chrome_major_version_number(
      version_info::GetMajorVersionNumberAsInt());
  capabilities->set_chrome_version(version_info::GetVersionNumber());

  // Pack payload into Any.
  browser_actuator::ActuatorUpstreamTypedPayload* typed_payload =
      upstream_message.add_typed_payloads();
  google::protobuf::Any* any_proto = typed_payload->mutable_proto_payload();
  any_proto->set_type_url(
      base::StrCat({"type.googleapis.com/", message.GetTypeName()}));
  any_proto->set_value(message.SerializeAsString());
  typed_payload->set_payload_type(proto_payload_type);

  // Return the populated `ActuatorUpstreamMessage` even when the network send
  // is rejected so `TransportChannelImpl` observers (e.g.
  // `SessionStreamRecorder`) still record the outgoing envelope under
  // `session_id`.
  const size_t max_pending_requests =
      static_cast<size_t>(std::max(1, kMaxPendingUpstreamRequests.Get()));
  if (!url_loader_factory_ || !endpoint_.is_valid() ||
      !net::IsUrlPotentiallyTrustworthy(endpoint_) ||
      pending_requests_.size() >= max_pending_requests ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*http_status_code=*/-1));
    return upstream_message;
  }

  browser_actuator::SendSessionMessageRequest request;
  request.set_request_id(base::Uuid::GenerateRandomV4().AsLowercaseString());
  *request.mutable_actuator_upstream_message() = upstream_message;

  UpstreamRequestLog metrics_log(payload_type);
  metrics_log.RecordPayloadSize(message.ByteSizeLong());

  auto pending = std::make_unique<PendingRequest>(
      request.SerializeAsString(), std::move(metrics_log), std::move(callback));
  PendingRequest* pending_ptr = pending.get();
  pending_requests_.emplace(pending_ptr, std::move(pending));

  StartAccessTokenFetch(pending_ptr);
  return upstream_message;
}

void UpstreamMessageClient::StartAccessTokenFetch(PendingRequest* pending) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending->access_token_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  pending->token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kBrowserActuator, identity_manager_,
          base::BindOnce(&UpstreamMessageClient::OnAccessTokenFetched,
                         weak_ptr_factory_.GetWeakPtr(), pending),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void UpstreamMessageClient::OnAccessTokenFetched(
    PendingRequest* pending,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending->token_fetcher.reset();

  if (error.state() != GoogleServiceAuthError::NONE ||
      pending->access_token_account_id.empty() ||
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
          pending->access_token_account_id) {
    CompleteRequest(pending, /*http_status_code=*/std::nullopt);
    return;
  }

  pending->access_token = std::move(access_token_info.token);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", pending->access_token}));

  pending->url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation_);
  if (pending->retried_auth_failure) {
    pending->url_loader->AttachStringForUpload(
        std::move(pending->serialized_request), "application/x-protobuf");
  } else {
    pending->url_loader->AttachStringForUpload(pending->serialized_request,
                                               "application/x-protobuf");
  }
  constexpr int kMax5xxRetries = 3;
  pending->url_loader->SetRetryOptions(kMax5xxRetries,
                                       network::SimpleURLLoader::RETRY_ON_5XX);
  pending->url_loader->SetTimeoutDuration(
      std::max(base::Seconds(1), kBrowserActuatorUpstreamTimeout.Get()));
  pending->url_loader->SetAllowHttpErrorResults(true);
  pending->url_loader->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&UpstreamMessageClient::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), pending));
}

void UpstreamMessageClient::OnSimpleLoaderComplete(
    PendingRequest* pending,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<int> http_status_code;
  if (pending->url_loader->NetError() == net::OK && headers) {
    http_status_code = headers->response_code();
  }

  if (http_status_code == net::HTTP_UNAUTHORIZED) {
    const CoreAccountId issuing_account_id = pending->access_token_account_id;
    pending->access_token_account_id = CoreAccountId();
    if (!pending->access_token.empty()) {
      identity_manager_->RemoveAccessTokenFromCache(
          issuing_account_id, signin::OAuthConsumerId::kBrowserActuator,
          pending->access_token);
      pending->access_token.clear();
    }
    if (!pending->retried_auth_failure && !issuing_account_id.empty() &&
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) ==
            issuing_account_id) {
      pending->retried_auth_failure = true;
      pending->url_loader.reset();
      StartAccessTokenFetch(pending);
      return;
    }
  }

  CompleteRequest(pending, http_status_code);
}

void UpstreamMessageClient::CompleteRequest(
    PendingRequest* pending,
    std::optional<int> http_status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = http_status_code.value_or(-1);

  pending->metrics_log.RecordResponse(http_status_code);

  auto it = pending_requests_.find(pending);
  CHECK(it != pending_requests_.end());
  std::unique_ptr<PendingRequest> owned_pending = std::move(it->second);
  pending_requests_.erase(it);

  std::move(owned_pending->callback).Run(response_code);
}

}  // namespace stream_transport
