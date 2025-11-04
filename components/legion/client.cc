// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/legion/attestation_handler_impl.h"
#include "components/legion/features.h"
#include "components/legion/proto/legion.pb.h"
#include "components/legion/secure_channel_impl.h"
#include "components/legion/secure_session_impl.h"
#include "components/legion/websocket_client.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace legion {

namespace {

void OnGenerateContentRequestCompleted(
    Client::OnTextRequestCompletedCallback cb,
    base::expected<proto::GenerateContentResponse, ErrorCode> result) {
  if (!result.has_value()) {
    std::move(cb).Run(base::unexpected(result.error()));
    return;
  }

  if (result->candidates_size() == 0 ||
      result->candidates(0).content().parts_size() == 0) {
    LOG(ERROR) << "GenerateContentResponse did not contain any content";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoContent));
    return;
  }

  std::move(cb).Run(result->candidates(0).content().parts(0).text());
}

void OnRequestSent(
    Client::OnGenerateContentRequestCompletedCallback cb,
    base::expected<Response, ErrorCode> result) {
  if (!result.has_value()) {
    std::move(cb).Run(base::unexpected(result.error()));
    return;
  }

  proto::LegionResponse legion_response;
  if (!legion_response.ParseFromArray(result->data(), result->size())) {
    LOG(ERROR) << "Failed to parse LegionResponse";
    std::move(cb).Run(base::unexpected(ErrorCode::kResponseParseError));
    return;
  }

  if (!legion_response.has_generate_content_response()) {
    LOG(ERROR) << "LegionResponse did not contain a "
                  "generate_content_response";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoResponse));
    return;
  }

  std::move(cb).Run(legion_response.generate_content_response());
}

}  // namespace

// static
Client Client::Create(network::mojom::NetworkContext* network_context,
                      proto::FeatureName feature_name) {
  auto url = GURL(base::StrCat({"wss://", legion::kLegionUrl.Get(),
                                "?key=", legion::kLegionApiKey.Get()}));
  return CreateWithUrl(url, network_context, feature_name);
}

// static
Client Client::CreateWithUrl(const GURL& url,
                             network::mojom::NetworkContext* network_context,
                             proto::FeatureName feature_name) {
  // Create dependencies for SecureChannelImpl.
  auto transport = std::make_unique<WebSocketClient>(
      url, base::BindRepeating(
               [](network::mojom::NetworkContext* context) { return context; },
               base::Unretained(network_context)));
  auto secure_session = std::make_unique<SecureSessionImpl>();
  auto attestation_handler = std::make_unique<AttestationHandlerImpl>();

  auto secure_channel = std::make_unique<SecureChannelImpl>(
      std::move(transport), std::move(secure_session),
      std::move(attestation_handler));

  return Client(std::move(secure_channel), feature_name);
}

Client::Client(std::unique_ptr<SecureChannel> secure_channel,
               proto::FeatureName feature_name)
    : secure_channel_(std::move(secure_channel)),
      feature_name_(feature_name) {
  CHECK(secure_channel_);
}

Client::~Client() = default;

Client::Client(Client&&) = default;
Client& Client::operator=(Client&&) = default;

void Client::SendRequest(
    Request request,
    base::OnceCallback<void(base::expected<Response, ErrorCode>)> callback) {
  DVLOG(1) << "SendRequest started.";

  DVLOG(1) << "Calling SecureChannelClient to execute the request.";
  // The SecureChannel is responsible for using the underlying
  // transport (WebSocketClient) to communicate with the service.
  secure_channel_->Write(std::move(request), std::move(callback));
}

void Client::SendTextRequest(const std::string& text,
                             OnTextRequestCompletedCallback callback) {
  proto::GenerateContentRequest request;
  auto* content = request.add_contents();
  content->set_role("user");
  auto* part = content->add_parts();
  part->set_text(text);

  auto text_response_callback =
      base::BindOnce(&OnGenerateContentRequestCompleted, std::move(callback));

  SendGenerateContentRequest(request, std::move(text_response_callback));
}

void Client::SendGenerateContentRequest(
    const proto::GenerateContentRequest& request,
    OnGenerateContentRequestCompletedCallback callback) {
  proto::LegionRequest request_proto;
  request_proto.set_feature_name(feature_name_);
  *request_proto.mutable_generate_content_request() = request;

  std::string serialized_request;
  request_proto.SerializeToString(&serialized_request);
  Request binary_encoded_proto_request(serialized_request.begin(),
                                         serialized_request.end());

  auto response_parsing_callback =
      base::BindOnce(&OnRequestSent, std::move(callback));

  SendRequest(std::move(binary_encoded_proto_request),
              std::move(response_parsing_callback));
}

}  // namespace legion
