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
#include "components/private_ai/connection_manager.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/proto_utils/generate_content_response_utils.h"

namespace private_ai {

namespace {

void ReceiveTextRequest(
    Client::OnTextRequestCompletedCallback cb,
    base::expected<proto::GenerateContentResponse, StatusCode> result) {
  if (!result.has_value()) {
    std::move(cb).Run(base::unexpected(result.error()));
    return;
  }

  auto text = ConvertGenerateContentResponseToText(*result);
  if (!text.has_value()) {
    LOG(ERROR) << "GenerateContentResponse did not contain any content";
    std::move(cb).Run(base::unexpected(StatusCode::kNoContent));
    return;
  }

  std::move(cb).Run(text.value());
}

void ReceiveGenerateContentResponse(
    Client::OnGenerateContentRequestCompletedCallback cb,
    base::expected<proto::PrivateAiResponse, StatusCode> private_ai_response) {
  if (!private_ai_response.has_value()) {
    std::move(cb).Run(base::unexpected(private_ai_response.error()));
    return;
  }

  if (!private_ai_response->has_generate_content_response()) {
    LOG(ERROR) << "PrivateAiResponse did not contain a "
                  "generate_content_response";
    std::move(cb).Run(base::unexpected(StatusCode::kNoResponse));
    return;
  }
  std::move(cb).Run(
      std::move(private_ai_response->generate_content_response()));
}

void ReceivePaicMessage(
    Client::OnPaicMessageRequestCompletedCallback cb,
    base::expected<proto::PrivateAiResponse, StatusCode> private_ai_response) {
  if (!private_ai_response.has_value()) {
    std::move(cb).Run(base::unexpected(private_ai_response.error()));
    return;
  }

  if (!private_ai_response->has_paic_response()) {
    LOG(ERROR) << "PrivateAiResponse did not contain a "
                  "paic_response";
    std::move(cb).Run(base::unexpected(StatusCode::kNoResponse));
    return;
  }
  std::move(cb).Run(std::move(private_ai_response->paic_response()));
}

}  // namespace

ClientImpl::ClientImpl(std::unique_ptr<ConnectionFactory> connection_factory,
                       PrivateAiLogger* logger)
    : logger_(logger),
      connection_manager_(
          std::make_unique<ConnectionManager>(std::move(connection_factory),
                                              logger_)) {
  CHECK(logger_);
}

ClientImpl::~ClientImpl() {
  connection_manager_.reset();
}

void ClientImpl::EstablishConnection() {
  connection_manager_->GetConnection();
}

void ClientImpl::SendTextRequest(proto::FeatureName feature_name,
                                 const std::string& text,
                                 OnTextRequestCompletedCallback callback,
                                 const RequestOptions& options) {
  proto::GenerateContentRequest request;
  if (feature_name ==
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT) {
    request.set_model("models/dev-v3-xs-sc-text");
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

  connection_manager_->GetConnection()->Send(
      std::move(private_ai_request), options.timeout,
      base::BindOnce(&ClientImpl::OnReponseReceived, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ClientImpl::OnReponseReceived(
    OnRequestCompletedCallback cb,
    base::expected<proto::PrivateAiResponse, StatusCode> private_ai_response) {
  if (private_ai_response.has_value()) {
    logger_->LogInfo(FROM_HERE, "Response received.");
  } else {
    logger_->LogError(FROM_HERE,
                      "Status: " + base::ToString(private_ai_response.error()));
  }
  std::move(cb).Run(private_ai_response);
}

}  // namespace private_ai
