// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals_page_handler.h"

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_utils.h"
#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals.mojom.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/optimization_guide/proto/model_execution.ostream.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/content/private_ai_network_driver_content.h"
#include "components/private_ai/content/private_ai_oak_session_driver_content.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/proto/private_ai.ostream.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

namespace {

template <typename ProtoResponseType>
base::expected<ProtoResponseType, std::string> ParseResponse(
    const base::expected<proto::PaicMessage, StatusCode>& response) {
  if (!response.has_value()) {
    return base::unexpected(
        std::string("Error: ") +
        base::NumberToString(static_cast<int>(response.error())));
  }

  if (!response->has_execute_response_ext()) {
    std::ostringstream err;
    err << "Error: no execute_response_ext: " << *response;
    return base::unexpected(err.str());
  }

  const auto& execute_response = response->execute_response_ext();
  if (execute_response.has_error_response()) {
    std::ostringstream err;
    err << "MES error_response: " << execute_response.error_response();
    return base::unexpected(err.str());
  }

  auto parsed_response =
      optimization_guide::ParsedAnyMetadata<ProtoResponseType>(
          execute_response.response_metadata());
  if (!parsed_response) {
    std::ostringstream err;
    err << "Error: failed to parse response metadata: " << execute_response;
    return base::unexpected(err.str());
  }

  return *parsed_response;
}

}  // namespace

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
                     network_context_, token_manager_, &webui_logger_,
                     &oak_session_driver_content_, &network_driver_content_);
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

  if (request.empty()) {
    webui_client_->EstablishConnection(feature_name_proto);
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->response = "Prewarming connection";
    std::move(callback).Run(std::move(result));
    return;
  }

  if (feature_name_proto == proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION) {
    SendZssRequest(request, std::move(callback));
    return;
  }

  if (feature_name_proto == proto::FEATURE_NAME_CHROME_FORMS_AI) {
    SendFormsAiRequest(request, std::move(callback));
    return;
  }

  if (feature_name_proto == proto::FEATURE_NAME_CHROME_CONTEXTUAL_CUEING) {
    SendContextualCueRequest(request, std::move(callback));
    return;
  }

  SendTextRequest(feature_name_proto, request, std::move(callback));
}

void PrivateAiInternalsPageHandler::SendTextRequest(
    proto::FeatureName feature_name,
    const std::string& request,
    SendRequestCallback callback) {
  if (!webui_client_) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  webui_client_->SendTextRequest(
      feature_name, request,
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
    SendRequestCallback callback) {
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
  PopulatePaicMessage(
      private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION,
      execute_request, &paic_message);

  webui_client_->SendPaicRequest(
      private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION,
      paic_message,
      base::BindOnce(
          [](SendRequestCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::StatusCode> response) {
            auto zss_response = ParseResponse<
                optimization_guide::proto::ZeroStateSuggestionsResponse>(
                response);
            auto result = private_ai_internals::mojom::PrivateAiResponse::New();
            if (!zss_response.has_value()) {
              result->error = std::move(zss_response.error());
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
    SendRequestCallback callback) {
  if (!webui_client_) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  optimization_guide::proto::AutofillAiTypeRequest forms_ai_request;
  forms_ai_request.mutable_page_context()->set_url(url);
  forms_ai_request.mutable_page_context()->set_title("");
  auto* form_data = forms_ai_request.mutable_form_data();
  form_data->set_form_name("");

  auto* to_field = form_data->add_fields();
  to_field->set_field_name("");
  to_field->set_field_label("To");
  to_field->set_placeholder("Airport or City");
  to_field->set_form_control_ax_node_id(0);
  to_field->set_aria_label("");
  to_field->set_aria_description("");

  auto* from_field = form_data->add_fields();
  from_field->set_field_name("");
  from_field->set_field_label("From");
  from_field->set_placeholder("Airport or City");
  from_field->set_form_control_ax_node_id(0);
  from_field->set_aria_label("");
  from_field->set_aria_description("");

  forms_ai_request.mutable_annotated_page_content();
  forms_ai_request.set_include_reasoning(true);

  optimization_guide::proto::ExecuteRequest execute_request;
  execute_request.set_feature(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS);
  *execute_request.mutable_request_metadata() =
      optimization_guide::AnyWrapProto(forms_ai_request);

  private_ai::proto::PaicMessage paic_message;
  PopulatePaicMessage(private_ai::proto::FEATURE_NAME_CHROME_FORMS_AI,
                      execute_request, &paic_message);

  webui_client_->SendPaicRequest(
      private_ai::proto::FEATURE_NAME_CHROME_FORMS_AI, paic_message,
      base::BindOnce(
          [](SendRequestCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::StatusCode> response) {
            auto forms_ai_response = ParseResponse<
                optimization_guide::proto::AutofillAiTypeResponse>(response);
            auto result = private_ai_internals::mojom::PrivateAiResponse::New();
            if (!forms_ai_response.has_value()) {
              result->error = std::move(forms_ai_response.error());
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

void PrivateAiInternalsPageHandler::SendContextualCueRequest(
    const std::string& request,
    SendRequestCallback callback) {
  if (!webui_client_) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  std::string url;
  std::string title;
  size_t first_newline = request.find('\n');
  if (first_newline == std::string::npos) {
    url = request;
  } else {
    url = request.substr(0, first_newline);
    title = request.substr(first_newline + 1);
  }
  base::TrimWhitespaceASCII(url, base::TRIM_ALL, &url);
  base::TrimWhitespaceASCII(title, base::TRIM_ALL, &title);

  optimization_guide::proto::ContextualCueingRequest contextual_cue_request;
  contextual_cue_request.mutable_active_tab_page_context()->set_url(url);
  contextual_cue_request.mutable_active_tab_page_context()->set_title(title);
  contextual_cue_request.add_supported_surfaces(
      optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_GEMINI_IN_CHROME);

  optimization_guide::proto::ExecuteRequest execute_request;
  execute_request.set_feature(optimization_guide::proto::ModelExecutionFeature::
                                  MODEL_EXECUTION_FEATURE_CONTEXTUAL_CUEING);
  *execute_request.mutable_request_metadata() =
      optimization_guide::AnyWrapProto(contextual_cue_request);

  private_ai::proto::PaicMessage paic_message;
  PopulatePaicMessage(private_ai::proto::FEATURE_NAME_CHROME_CONTEXTUAL_CUEING,
                      execute_request, &paic_message);

  webui_client_->SendPaicRequest(
      private_ai::proto::FEATURE_NAME_CHROME_CONTEXTUAL_CUEING, paic_message,
      base::BindOnce(
          [](SendRequestCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::StatusCode> response) {
            auto contextual_cue_response = ParseResponse<
                optimization_guide::proto::ContextualCueingResponse>(response);
            auto result = private_ai_internals::mojom::PrivateAiResponse::New();
            if (!contextual_cue_response.has_value()) {
              result->error = std::move(contextual_cue_response.error());
              std::move(callback).Run(std::move(result));
              return;
            }

            std::vector<std::string> cues;
            if (contextual_cue_response->has_anchored_message_cue()) {
              cues.push_back("Deprecated CUJ: " +
                             contextual_cue_response->suggested_cuj() +
                             ", Msg: " +
                             contextual_cue_response->anchored_message_cue()
                                 .anchored_message_text() +
                             ", Action: " +
                             contextual_cue_response->anchored_message_cue()
                                 .action_text());
            }
            for (const auto& cue : contextual_cue_response->contextual_cues()) {
              std::string cue_info = "CUJ: " + cue.suggested_cuj();
              if (cue.has_anchored_message_cue()) {
                cue_info +=
                    ", Msg: " +
                    cue.anchored_message_cue().anchored_message_text() +
                    ", Action: " + cue.anchored_message_cue().action_text();
              }
              cues.push_back(cue_info);
            }

            result->response = base::JoinString(cues, "; ");
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
