// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client_impl.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/to_string.h"
#include "components/legion/common/legion_logger.h"
#include "components/legion/connection.h"
#include "components/legion/proto/legion.pb.h"
#include "components/legion/proto_utils/generate_content_response_utils.h"

namespace legion {

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
    base::expected<proto::LegionResponse, ErrorCode> legion_response) {
  if (!legion_response.has_value()) {
    std::move(cb).Run(base::unexpected(legion_response.error()));
    return;
  }

  if (!legion_response->has_generate_content_response()) {
    LOG(ERROR) << "LegionResponse did not contain a "
                  "generate_content_response";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoResponse));
    return;
  }
  std::move(cb).Run(std::move(legion_response->generate_content_response()));
}

void ReceivePaicMessage(
    Client::OnPaicMessageRequestCompletedCallback cb,
    base::expected<proto::LegionResponse, ErrorCode> legion_response) {
  if (!legion_response.has_value()) {
    std::move(cb).Run(base::unexpected(legion_response.error()));
    return;
  }

  if (!legion_response->has_paic_response()) {
    LOG(ERROR) << "LegionResponse did not contain a "
                  "paic_response";
    std::move(cb).Run(base::unexpected(ErrorCode::kNoResponse));
    return;
  }
  std::move(cb).Run(std::move(legion_response->paic_response()));
}

}  // namespace

ClientImpl::ClientImpl(std::unique_ptr<ConnectionFactory> connection_factory,
                       std::unique_ptr<LegionLogger> logger)
    : logger_(std::move(logger)),
      connection_factory_(std::move(connection_factory)) {
  CHECK(logger_);
}

ClientImpl::~ClientImpl() = default;

void ClientImpl::EstablishSession(
    OnEstablishSessionCompletedCallback callback) {
  GetOrCreateConnection();
  std::move(callback).Run(base::ok());
}

Connection* ClientImpl::GetOrCreateConnection() {
  if (!connection_) {
    connection_ = connection_factory_->Create(base::BindRepeating(
        &ClientImpl::OnConnectionDisconnected, base::Unretained(this)));
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

LegionLogger* ClientImpl::GetLogger() {
  return logger_.get();
}

void ClientImpl::SendGenerateContentRequest(
    proto::FeatureName feature_name,
    const proto::GenerateContentRequest& request,
    OnGenerateContentRequestCompletedCallback callback,
    const RequestOptions& options) {
  proto::LegionRequest request_proto;
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
  proto::LegionRequest legion_request;
  *legion_request.mutable_paic_request() = request;

  auto response_callback =
      base::BindOnce(&ReceivePaicMessage, std::move(callback));

  SendRequest(feature_name, std::move(legion_request),
              std::move(response_callback), options);
}

void ClientImpl::SendRequest(proto::FeatureName feature_name,
                             proto::LegionRequest legion_request,
                             OnRequestCompletedCallback callback,
                             const RequestOptions& options) {
  logger_->LogInfo(FROM_HERE, "SendRequest started.");

  legion_request.set_feature_name(feature_name);

  GetOrCreateConnection()->Send(
      std::move(legion_request), options.timeout,
      base::BindOnce(&ClientImpl::OnReponseReceived, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ClientImpl::OnReponseReceived(
    OnRequestCompletedCallback cb,
    base::expected<proto::LegionResponse, ErrorCode> legion_response) {
  if (legion_response.has_value()) {
    logger_->LogInfo(FROM_HERE, "Response received");
  } else {
    logger_->LogError(FROM_HERE,
                      "Error: " + base::ToString(legion_response.error()));
  }
  std::move(cb).Run(legion_response);
}

void ClientImpl::OnConnectionDisconnected() {
  logger_->LogInfo(FROM_HERE,
                   "Connection disconnected. Destroying connection.");
  connection_.reset();
}

}  // namespace legion
