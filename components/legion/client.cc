// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
    base::expected<Client::BinaryEncodedProtoResponse, ErrorCode> result) {
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
std::unique_ptr<Client> Client::Create(
    network::mojom::NetworkContext* network_context) {
  return CreateWithUrl(
      FormatUrl(legion::kLegionUrl.Get(), legion::kLegionApiKey.Get()),
      network_context);
}

// static
std::unique_ptr<Client> Client::CreateWithUrl(
    const GURL& url,
    network::mojom::NetworkContext* network_context) {
  if (!base::FeatureList::IsEnabled(kLegion)) {
    return nullptr;
  }

  auto factory = base::BindRepeating(
      [](const GURL& url, network::mojom::NetworkContext* context)
          -> std::unique_ptr<SecureChannel> {
        auto transport = std::make_unique<WebSocketClient>(
            url,
            base::BindRepeating(
                [](network::mojom::NetworkContext* context) { return context; },
                base::Unretained(context)));
        auto secure_session = std::make_unique<SecureSessionImpl>();
        auto attestation_handler = std::make_unique<AttestationHandlerImpl>();

        return std::make_unique<SecureChannelImpl>(
            std::move(transport), std::move(secure_session),
            std::move(attestation_handler));
      },
      url, base::Unretained(network_context));

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(new Client(std::move(factory)));
}

// static
GURL Client::FormatUrl(const std::string& url, const std::string& api_key) {
  return GURL(base::StrCat({"wss://", url, "?key=", api_key}));
}

Client::Client(SecureChannelFactory channel_factory)
    : secure_channel_factory_(std::move(channel_factory)) {
  RecreateSecureChannel();
}

Client::~Client() = default;

void Client::RecreateSecureChannel() {
  secure_channel_ = secure_channel_factory_.Run();
  secure_channel_->SetResponseCallback(
      base::BindRepeating(&Client::OnResponseReceived, base::Unretained(this)));
}

void Client::SendRequest(int32_t request_id,
                         BinaryEncodedProtoRequest request,
                         OnRequestCompletedCallback callback) {
  DVLOG(1) << "SendRequest started.";

  if (secure_channel_->Write(request)) {
    pending_requests_.emplace(request_id, std::move(callback));
    return;
  }

  // The channel is in a permanent failure state, so fail the current request.
  DVLOG(1) << "Secure channel write failed.";
  std::move(callback).Run(base::unexpected(ErrorCode::kError));
}

void Client::SendTextRequest(proto::FeatureName feature_name,
                             const std::string& text,
                             OnTextRequestCompletedCallback callback) {
  proto::GenerateContentRequest request;
  if (feature_name ==
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT) {
    request.set_model("dev_v3xs");
  }
  auto* content = request.add_contents();
  content->set_role("user");
  auto* part = content->add_parts();
  part->set_text(text);

  auto text_response_callback =
      base::BindOnce(&OnGenerateContentRequestCompleted, std::move(callback));

  SendGenerateContentRequest(feature_name, request,
                             std::move(text_response_callback));
}

void Client::SendGenerateContentRequest(
    proto::FeatureName feature_name,
    const proto::GenerateContentRequest& request,
    OnGenerateContentRequestCompletedCallback callback) {
  int32_t request_id = next_request_id_;
  next_request_id_++;

  proto::LegionRequest request_proto;
  request_proto.set_feature_name(feature_name);
  request_proto.set_request_id(request_id);
  *request_proto.mutable_generate_content_request() = request;

  std::string serialized_request;
  request_proto.SerializeToString(&serialized_request);
  BinaryEncodedProtoRequest binary_encoded_proto_request(
      serialized_request.begin(), serialized_request.end());

  auto response_parsing_callback =
      base::BindOnce(&OnRequestSent, std::move(callback));

  SendRequest(request_id, std::move(binary_encoded_proto_request),
              std::move(response_parsing_callback));
}

void Client::FailAllPendingRequests(ErrorCode error_code) {
  auto pending_requests = std::move(pending_requests_);
  for (auto& entry : pending_requests) {
    std::move(entry.second).Run(base::unexpected(error_code));
  }
}

void Client::OnResponseReceived(
    base::expected<BinaryEncodedProtoResponse, ErrorCode> result) {
  if (!result.has_value()) {
    // The secure channel is broken. Fail all pending requests and recreate the
    // channel.
    DVLOG(1) << "Secure channel read failed. Recreating channel.";
    FailAllPendingRequests(result.error());
    RecreateSecureChannel();
    return;
  }

  proto::LegionResponse legion_response;
  if (!legion_response.ParseFromArray(result->data(), result->size())) {
    LOG(ERROR) << "Failed to parse LegionResponse";
    // This is a protocol error. We don't know which request this response was
    // for, so we fail all of them.
    FailAllPendingRequests(ErrorCode::kResponseParseError);
    return;
  }

  auto it = pending_requests_.find(legion_response.request_id());
  if (it == pending_requests_.end()) {
    DLOG(ERROR) << "Received response for unknown request_id: "
                << legion_response.request_id();
    // This could be a response to a request that has already timed out and was
    // removed from the pending list. In this case we should just ignore it and
    // not cancel other pending requests.
    return;
  }

  auto callback = std::move(it->second);
  pending_requests_.erase(it);

  std::move(callback).Run(std::move(result));
}

}  // namespace legion
