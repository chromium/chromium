// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace browser_actuator {

namespace {

ActuatorUpstreamPayloadType ToProtoPayloadType(PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kUnspecified:
      CHECK(false);
    case PayloadType::kControl:
      return ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND;
    case PayloadType::kExperimentalTriggering:
      return ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING;
  }
  NOTREACHED();
}

}  // namespace

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

void UpstreamMessageClient::SendUpstreamMessage(
    std::string_view session_id,
    int64_t client_sequence_number,
    std::optional<int64_t> responding_to_sequence_number,
    PayloadType payload_type,
    const google::protobuf::MessageLite& message,
    SendCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id(session_id);
  upstream.set_client_sequence_number(client_sequence_number);
  if (responding_to_sequence_number.has_value()) {
    upstream.set_responding_to_sequence_number(*responding_to_sequence_number);
  }

  // Pack payload into Any.
  ActuatorUpstreamTypedPayload* typed_payload = upstream.add_typed_payloads();
  google::protobuf::Any* any_proto = typed_payload->mutable_proto_payload();
  any_proto->set_type_url(
      base::StrCat({"type.googleapis.com/", message.GetTypeName()}));
  any_proto->set_value(message.SerializeAsString());
  typed_payload->set_payload_type(ToProtoPayloadType(payload_type));

  SendSessionMessageRequest request;
  request.set_request_id(base::Uuid::GenerateRandomV4().AsLowercaseString());
  *request.mutable_actuator_upstream_message() = std::move(upstream);

  endpoint_fetcher::EndpointFetcher::RequestParams::Builder params_builder(
      endpoint_fetcher::HttpMethod::kPost, traffic_annotation_);
  params_builder.SetUrl(endpoint_)
      .SetPostData(request.SerializeAsString())
      .SetContentType("application/x-protobuf")
      .SetAuthType(endpoint_fetcher::AuthType::OAUTH)
      .SetOAuthConsumerId(signin::OAuthConsumerId::kBrowserActuator)
      .SetConsentLevel(signin::ConsentLevel::kSignin);

  auto fetcher = std::make_unique<endpoint_fetcher::EndpointFetcher>(
      url_loader_factory_, identity_manager_, params_builder.Build());

  auto* fetcher_ptr = fetcher.get();
  pending_requests_.emplace(fetcher_ptr, std::move(fetcher));

  fetcher_ptr->Fetch(base::BindOnce(&UpstreamMessageClient::OnMessageSent,
                                    weak_ptr_factory_.GetWeakPtr(), fetcher_ptr,
                                    std::move(callback)));
}

void UpstreamMessageClient::OnMessageSent(
    endpoint_fetcher::EndpointFetcher* fetcher,
    SendCompleteCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = response ? response->http_status_code : -1;
  bool success = response_code == net::HTTP_OK;

  // Move the fetcher out of pending_requests_ so it is owned by local stack
  // and stays alive until this function returns, even if `callback` destroys
  // `this`.
  auto it = pending_requests_.find(fetcher);
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> owned_fetcher;
  if (it != pending_requests_.end()) {
    owned_fetcher = std::move(it->second);
    pending_requests_.erase(it);
  }

  std::move(callback).Run(success, response_code);
}

}  // namespace browser_actuator
