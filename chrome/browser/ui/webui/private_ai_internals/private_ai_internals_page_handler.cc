// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals_page_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals.mojom.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

PrivateAiInternalsPageHandler::PrivateAiInternalsPageHandler(
    phosphor::TokenManager* token_manager,
    network::mojom::NetworkContext* network_context,
    Client* private_ai_client,
    PrivateAiLogger* private_ai_logger,
    mojo::PendingReceiver<
        private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver)
    : token_manager_(token_manager),
      private_ai_client_(private_ai_client),
      private_ai_logger_(private_ai_logger),
      network_context_(network_context),
      receiver_(this, std::move(receiver)) {
  CHECK(private_ai_logger_);

  scoped_logger_observations_.AddObservation(private_ai_logger_.get());
  scoped_logger_observations_.AddObservation(&webui_logger_);
}

PrivateAiInternalsPageHandler::~PrivateAiInternalsPageHandler() = default;

void PrivateAiInternalsPageHandler::SetPage(
    mojo::PendingRemote<private_ai_internals::mojom::PrivateAiInternalsPage>
        page) {
  page_.Bind(std::move(page));
}

void PrivateAiInternalsPageHandler::Connect(const std::string& url,
                                            const std::string& api_key,
                                            const std::string& proxy_url,
                                            bool use_token_attestation,
                                            ConnectCallback callback) {
  std::string effective_api_key = api_key;
  if (effective_api_key == kApiKeyPlaceholder) {
    effective_api_key = PrivateAiService::GetApiKey();
  }

  webui_client_ =
      Client::Create(url, effective_api_key, proxy_url, use_token_attestation,
                     network_context_, token_manager_, &webui_logger_);
  webui_client_->EstablishConnection();
  std::move(callback).Run();
}

void PrivateAiInternalsPageHandler::Close(CloseCallback callback) {
  webui_client_.reset();
  std::move(callback).Run();
}

void PrivateAiInternalsPageHandler::SendRequest(const std::string& feature_name,
                                                const std::string& request,
                                                SendRequestCallback callback) {
  if (!webui_client_) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  proto::FeatureName feature_name_proto;
  if (!proto::FeatureName_Parse(feature_name, &feature_name_proto)) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: invalid feature_name: ") + feature_name;
    std::move(callback).Run(std::move(result));
    return;
  }

  webui_client_->SendTextRequest(
      feature_name_proto, request,
      base::BindOnce(
          [](SendRequestCallback callback,
             base::expected<std::string, StatusCode> response) {
            auto result = private_ai_internals::mojom::PrivateAiResponse::New();
            if (response.has_value()) {
              result->response = *response;
            } else {
              result->error =
                  std::string("Error: ") +
                  base::NumberToString(static_cast<int>(response.error()));
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)),
      /*options=*/{});
}

void PrivateAiInternalsPageHandler::SendZssRequest(
    const std::string& inner_text,
    SendZssRequestCallback callback) {
  if (!webui_client_) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  optimization_guide::proto::ZeroStateSuggestionsRequest zss_request;
  zss_request.mutable_page_context()->set_inner_text(inner_text);

  optimization_guide::proto::ExecuteRequest execute_request;
  execute_request.set_feature(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ZERO_STATE_SUGGESTIONS);
  *execute_request.mutable_request_metadata() =
      optimization_guide::AnyWrapProto(zss_request);

  private_ai::proto::PaicMessage paic_message;
  paic_message.set_feature_name(
      private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  *paic_message.mutable_execute_request_ext() = execute_request;

  webui_client_->SendPaicRequest(
      private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION,
      paic_message,
      base::BindOnce(
          [](SendZssRequestCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::StatusCode> response) {
            auto result = private_ai_internals::mojom::PrivateAiResponse::New();
            if (!response.has_value()) {
              result->error =
                  std::string("Error: ") +
                  base::NumberToString(static_cast<int>(response.error()));
              std::move(callback).Run(std::move(result));
              return;
            }

            if (!response->has_execute_response_ext()) {
              result->error = "Error: no execute_response_ext";
              std::move(callback).Run(std::move(result));
              return;
            }

            auto zss_response = optimization_guide::ParsedAnyMetadata<
                optimization_guide::proto::ZeroStateSuggestionsResponse>(
                response->execute_response_ext().response_metadata());

            if (!zss_response) {
              result->error = "Error: failed to parse zss response";
              std::move(callback).Run(std::move(result));
              return;
            }

            std::vector<std::string> labels;
            for (const auto& suggestion : zss_response->suggestions()) {
              labels.push_back(suggestion.label());
            }

            result->response = base::JoinString(labels, ", ");
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)),
      /*options=*/{});
}

void PrivateAiInternalsPageHandler::SendFormsAiRequest(
    const std::string& url,
    SendFormsAiRequestCallback callback) {
  if (!webui_client_) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  optimization_guide::proto::AutofillAiTypeRequest forms_ai_request;
  forms_ai_request.set_url(url);
  auto* form_data = forms_ai_request.mutable_form_data();
  form_data->set_form_name("webui_test_form");
  auto* field = form_data->add_fields();
  field->set_field_name("first_name");
  field->set_field_label("First Name");

  optimization_guide::proto::ExecuteRequest execute_request;
  execute_request.set_feature(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS);
  *execute_request.mutable_request_metadata() =
      optimization_guide::AnyWrapProto(forms_ai_request);

  private_ai::proto::PaicMessage paic_message;
  paic_message.set_feature_name(
      private_ai::proto::FEATURE_NAME_CHROME_FORMS_AI);
  *paic_message.mutable_execute_request_ext() = execute_request;

  webui_client_->SendPaicRequest(
      private_ai::proto::FEATURE_NAME_CHROME_FORMS_AI, paic_message,
      base::BindOnce(
          [](SendFormsAiRequestCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::StatusCode> response) {
            auto result = private_ai_internals::mojom::PrivateAiResponse::New();
            if (!response.has_value()) {
              result->error =
                  std::string("Error: ") +
                  base::NumberToString(static_cast<int>(response.error()));
              std::move(callback).Run(std::move(result));
              return;
            }

            if (!response->has_execute_response_ext()) {
              result->error = "Error: no execute_response_ext";
              std::move(callback).Run(std::move(result));
              return;
            }

            auto forms_ai_response = optimization_guide::ParsedAnyMetadata<
                optimization_guide::proto::AutofillAiTypeResponse>(
                response->execute_response_ext().response_metadata());

            if (!forms_ai_response) {
              result->error = "Error: failed to parse forms ai response";
              std::move(callback).Run(std::move(result));
              return;
            }

            std::vector<std::string> types;
            for (const auto& field_response :
                 forms_ai_response->field_responses()) {
              types.push_back(
                  base::NumberToString(field_response.field_type()));
            }

            result->response = base::JoinString(types, ", ");
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)),
      /*options=*/{});
}

void PrivateAiInternalsPageHandler::OnLogInfo(const base::Location& location,
                                              std::string_view message) {
  LogToPage(private_ai_internals::mojom::LogLevel::kInfo, location, message);
}

void PrivateAiInternalsPageHandler::OnLogError(const base::Location& location,
                                               std::string_view message) {
  LogToPage(private_ai_internals::mojom::LogLevel::kError, location, message);
}

void PrivateAiInternalsPageHandler::LogToPage(
    private_ai_internals::mojom::LogLevel level,
    const base::Location& location,
    std::string_view message) {
  if (!page_) {
    return;
  }
  // TODO(crbug.com/461435924): Make it possible to differentiate logs from
  // the webui client and logs from the profile client.
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  std::string timestamp = base::StringPrintf(
      "[%02d:%02d:%02d.%03d] ", exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);

  page_->OnLogMessage(level, timestamp + location.function_name() + ": " +
                                 std::string(message));
}

}  // namespace private_ai
