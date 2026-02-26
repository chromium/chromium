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
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals.mojom.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

PrivateAiInternalsPageHandler::PrivateAiInternalsPageHandler(
    private_ai::phosphor::TokenManager* token_manager,
    network::mojom::NetworkContext* network_context,
    private_ai::Client* private_ai_client,
    mojo::PendingReceiver<
        private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver)
    : token_manager_(token_manager),
      private_ai_client_(private_ai_client),
      network_context_(network_context),
      receiver_(this, std::move(receiver)) {}

PrivateAiInternalsPageHandler::~PrivateAiInternalsPageHandler() = default;

void PrivateAiInternalsPageHandler::SetPage(
    mojo::PendingRemote<private_ai_internals::mojom::PrivateAiInternalsPage>
        page) {
  page_.Bind(std::move(page));
  if (private_ai_client_) {
    scoped_logger_observations_.AddObservation(private_ai_client_->GetLogger());
  }
}

void PrivateAiInternalsPageHandler::Connect(const std::string& url,
                                            const std::string& api_key,
                                            const std::string& proxy_url,
                                            bool use_token_attestation,
                                            ConnectCallback callback) {
  webui_client_ = private_ai::Client::Create(
      url, api_key, proxy_url, use_token_attestation, network_context_,
      token_manager_, content::GetNetworkService(),
      std::make_unique<private_ai::PrivateAiLogger>());
  scoped_logger_observations_.AddObservation(webui_client_->GetLogger());
  webui_client_->EstablishConnection();
  std::move(callback).Run();
}

void PrivateAiInternalsPageHandler::Close(CloseCallback callback) {
  if (webui_client_) {
    scoped_logger_observations_.RemoveObservation(webui_client_->GetLogger());
    webui_client_.reset();
  }
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

  private_ai::proto::FeatureName feature_name_proto;
  if (!private_ai::proto::FeatureName_Parse(feature_name,
                                            &feature_name_proto)) {
    auto result = private_ai_internals::mojom::PrivateAiResponse::New();
    result->error = std::string("Error: invalid feature_name: ") + feature_name;
    std::move(callback).Run(std::move(result));
    return;
  }

  webui_client_->SendTextRequest(
      feature_name_proto, request,
      base::BindOnce(
          [](SendRequestCallback callback,
             base::expected<std::string, private_ai::ErrorCode> response) {
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
