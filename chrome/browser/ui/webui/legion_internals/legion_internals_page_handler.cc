// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/legion_internals/legion_internals_page_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "components/legion/client.h"
#include "components/legion/common/legion_logger.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/token_manager.h"
#include "components/legion/proto/legion.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

LegionInternalsPageHandler::LegionInternalsPageHandler(
    legion::phosphor::TokenManager* token_manager,
    network::mojom::NetworkContext* network_context,
    mojo::PendingReceiver<legion_internals::mojom::LegionInternalsPageHandler>
        receiver)
    : token_manager_(token_manager),
      network_context_(network_context),
      receiver_(this, std::move(receiver)) {}

LegionInternalsPageHandler::~LegionInternalsPageHandler() = default;

void LegionInternalsPageHandler::SetPage(
    mojo::PendingRemote<legion_internals::mojom::LegionInternalsPage> page) {
  page_.Bind(std::move(page));
}

void LegionInternalsPageHandler::Connect(const std::string& url,
                                         const std::string& api_key,
                                         ConnectCallback callback) {
  if (!api_key.empty()) {
    client_ = legion::Client::CreateWithApiKey(
        legion::Client::FormatUrl(url, api_key), network_context_);
  } else {
    client_ = legion::Client::CreateWithToken(legion::Client::FormatUrl(url),
                                              network_context_, token_manager_);
  }
  scoped_logger_observation_.Observe(client_->GetLogger());
  std::move(callback).Run();
}

void LegionInternalsPageHandler::Close(CloseCallback callback) {
  scoped_logger_observation_.Reset();
  client_.reset();
  std::move(callback).Run();
}

void LegionInternalsPageHandler::SendRequest(const std::string& feature_name,
                                             const std::string& request,
                                             SendRequestCallback callback) {
  if (!client_) {
    auto result = legion_internals::mojom::LegionResponse::New();
    result->error = std::string("Error: not connected");
    std::move(callback).Run(std::move(result));
    return;
  }

  legion::proto::FeatureName feature_name_proto;
  if (!legion::proto::FeatureName_Parse(feature_name, &feature_name_proto)) {
    auto result = legion_internals::mojom::LegionResponse::New();
    result->error = std::string("Error: invalid feature_name: ") + feature_name;
    std::move(callback).Run(std::move(result));
    return;
  }

  client_->SendTextRequest(
      feature_name_proto, request,
      base::BindOnce(
          [](SendRequestCallback callback,
             base::expected<std::string, legion::ErrorCode> response) {
            auto result = legion_internals::mojom::LegionResponse::New();
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

void LegionInternalsPageHandler::OnLogInfo(const base::Location& location,
                                           std::string_view message) {
  LogToPage(legion_internals::mojom::LogLevel::kInfo, location, message);
}

void LegionInternalsPageHandler::OnLogError(const base::Location& location,
                                            std::string_view message) {
  LogToPage(legion_internals::mojom::LogLevel::kError, location, message);
}

void LegionInternalsPageHandler::LogToPage(
    legion_internals::mojom::LogLevel level,
    const base::Location& location,
    std::string_view message) {
  if (!page_) {
    return;
  }

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  std::string timestamp = base::StringPrintf(
      "[%02d:%02d:%02d.%03d] ", exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);

  page_->OnLogMessage(level, timestamp + location.function_name() + ": " +
                                 std::string(message));
}
