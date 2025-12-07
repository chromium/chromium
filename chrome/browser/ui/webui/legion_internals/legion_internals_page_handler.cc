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
#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "components/legion/client.h"
#include "components/legion/features.h"
#include "components/legion/proto/legion.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

LegionInternalsPageHandler::LegionInternalsPageHandler(
    network::mojom::NetworkContext* network_context,
    mojo::PendingReceiver<legion_internals::mojom::LegionInternalsPageHandler>
        receiver)
    : network_context_(network_context), receiver_(this, std::move(receiver)) {}

LegionInternalsPageHandler::~LegionInternalsPageHandler() = default;

void LegionInternalsPageHandler::Connect(const std::string& url,
                                         const std::string& api_key,
                                         ConnectCallback callback) {
  GURL api_url = legion::Client::FormatUrl(url, api_key);
  client_ = legion::Client::CreateWithUrl(api_url, network_context_);
  std::move(callback).Run();
}

void LegionInternalsPageHandler::Close(CloseCallback callback) {
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
          std::move(callback)));
}
