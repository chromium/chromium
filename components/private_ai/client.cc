// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "components/private_ai/client_impl.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection_factory_impl.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace private_ai {

// static
std::unique_ptr<Client> Client::Create(
    const std::string& url,
    const std::string& api_key,
    const std::string& proxy_url_string,
    bool use_token_attestation,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager,
    network::mojom::NetworkService* network_service,
    std::unique_ptr<PrivateAiLogger> logger) {
  CHECK(!api_key.empty());
  GURL formatted_url = Client::FormatUrl(url, api_key);

  auto connection_factory = std::make_unique<ConnectionFactoryImpl>(
      formatted_url, network_context, logger.get());

  if (use_token_attestation) {
    connection_factory->EnableTokenAttestation(token_manager);
  }

  if (!proxy_url_string.empty()) {
    GURL proxy_url(proxy_url_string);
    if (!proxy_url.SchemeIsHTTPOrHTTPS()) {
      proxy_url = GURL(base::StrCat({"https://", proxy_url_string}));
    }
    connection_factory->EnableProxy(proxy_url, network_service);
  }

  return base::WrapUnique(
      new ClientImpl(std::move(connection_factory), std::move(logger)));
}

// static
GURL Client::FormatUrl(const std::string& url) {
  return GURL(base::StrCat({"wss://", url}));
}

// static
GURL Client::FormatUrl(const std::string& url, const std::string& api_key) {
  return GURL(base::StrCat({"wss://", url, "?key=", api_key}));
}

}  // namespace private_ai
