// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/to_string.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/proto_utils/generate_content_response_utils.h"

namespace private_ai {

namespace {

void ReceiveTextRequest(
    Client::OnTextRequestCompletedCallback cb,
    base::expected<proto::GenerateContentResponse, ErrorCode> result) {
  if (!result.has_value()) {
    std::move(cb).Run(base::unexpected(result.error()));
    return;
  }

  auto text = ConvertGenerateContentResponseToText(*result);
  if (!text.has_value()) {
    LOG(ERROR) << "GenerateContentResponse did not contain any content";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoContent));
    return;
  }

  std::move(cb).Run(text.value());
}

void ReceiveGenerateContentResponse(
    Client::OnGenerateContentRequestCompletedCallback cb,
    base::expected<proto::PrivateAiResponse, ErrorCode> private_ai_response) {
  if (!private_ai_response.has_value()) {
    std::move(cb).Run(base::unexpected(private_ai_response.error()));
    return;
  }

  if (!private_ai_response->has_generate_content_response()) {
    LOG(ERROR) << "PrivateAiResponse did not contain a "
                  "generate_content_response";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoResponse));
    return;
  }
  std::move(cb).Run(
      std::move(private_ai_response->generate_content_response()));
}

void ReceivePaicMessage(
    Client::OnPaicMessageRequestCompletedCallback cb,
    base::expected<proto::PrivateAiResponse, ErrorCode> private_ai_response) {
  if (!private_ai_response.has_value()) {
    std::move(cb).Run(base::unexpected(private_ai_response.error()));
    return;
  }

  if (!private_ai_response->has_paic_response()) {
    LOG(ERROR) << "PrivateAiResponse did not contain a "
                  "paic_response";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoResponse));
    return;
  }
  std::move(cb).Run(std::move(private_ai_response->paic_response()));
}

}  // namespace

ClientImpl::ClientImpl(std::unique_ptr<ConnectionFactory> connection_factory,
                       std::unique_ptr<PrivateAiLogger> logger)
    : logger_(std::move(logger)),
      connection_factory_(std::move(connection_factory)) {
  CHECK(logger_);
}

ClientImpl::~ClientImpl() {
  if (connection_) {
    connection_->OnDestroy(ErrorCode::kDestroyed);
  }
}

void ClientImpl::EstablishConnection() {
  GetOrCreateConnection();
}

Connection* ClientImpl::GetOrCreateConnection() {
  if (!connection_) {
    connection_ = connection_factory_->Create(base::BindOnce(
        &ClientImpl::OnConnectionDisconnected, weak_factory_.GetWeakPtr()));
  }
  return connection_.get();
}

void ClientImpl::SendTextRequest(proto::FeatureName feature_name,
                                 const std::string& text,
                                 OnTextRequestCompletedCallback callback,
                                 const RequestOptions& options) {
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
      base::BindOnce(&ReceiveTextRequest, std::move(callback));

  SendGenerateContentRequest(feature_name, request,
                             std::move(text_response_callback), options);
}

PrivateAiLogger* ClientImpl::GetLogger() {
  return logger_.get();
}

void ClientImpl::SendGenerateContentRequest(
    proto::FeatureName feature_name,
    const proto::GenerateContentRequest& request,
    OnGenerateContentRequestCompletedCallback callback,
    const RequestOptions& options) {
  proto::PrivateAiRequest request_proto;
  *request_proto.mutable_generate_content_request() = request;

  auto response_callback =
      base::BindOnce(&ReceiveGenerateContentResponse, std::move(callback));

  SendRequest(feature_name, std::move(request_proto),
              std::move(response_callback), options);
}

void ClientImpl::SendPaicRequest(proto::FeatureName feature_name,
                                 const proto::PaicMessage& request,
                                 OnPaicMessageRequestCompletedCallback callback,
                                 const RequestOptions& options) {
  proto::PrivateAiRequest private_ai_request;
  *private_ai_request.mutable_paic_request() = request;

  auto response_callback =
      base::BindOnce(&ReceivePaicMessage, std::move(callback));

  SendRequest(feature_name, std::move(private_ai_request),
              std::move(response_callback), options);
}

void ClientImpl::SendRequest(proto::FeatureName feature_name,
                             proto::PrivateAiRequest private_ai_request,
                             OnRequestCompletedCallback callback,
                             const RequestOptions& options) {
  logger_->LogInfo(FROM_HERE, "SendRequest started.");

  private_ai_request.set_feature_name(feature_name);

  GetOrCreateConnection()->Send(
      std::move(private_ai_request), options.timeout,
      base::BindOnce(&ClientImpl::OnReponseReceived, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ClientImpl::OnReponseReceived(
    OnRequestCompletedCallback cb,
    base::expected<proto::PrivateAiResponse, ErrorCode> private_ai_response) {
  if (private_ai_response.has_value()) {
    logger_->LogInfo(FROM_HERE, "Response received.");
  } else {
    logger_->LogError(FROM_HERE,
                      "Error: " + base::ToString(private_ai_response.error()));
  }
  std::move(cb).Run(private_ai_response);
}

void ClientImpl::OnConnectionDisconnected(ErrorCode error_code) {
  CHECK(connection_);
  logger_->LogInfo(
      FROM_HERE, "Connection disconnected. Destroying connection with error: " +
                     base::ToString(error_code));

  // Remove the reference to this Connection object to ensure that any
  // attempt at sending new requests from response handlers will create
  // a new Connection.
  auto connection = std::move(connection_);
  connection->OnDestroy(error_code);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<Connection> connection) {
                       // Release the connection asynchronously to avoid
                       // use-after-free inside this callback.
                     },
                     std::move(connection)));
}

}  // namespace private_ai
