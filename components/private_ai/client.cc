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
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace private_ai {

// static
std::unique_ptr<Client> Client::Create(
    const std::string& url,
    const std::string& api_key,
    const std::string& proxy_url_string,
    bool use_token_attestation,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager,
    PrivateAiLogger* logger,
    PrivateAiOakSessionDriver* oak_session_driver,
    PrivateAiNetworkDriver* network_driver,
    version_info::Channel channel) {
  CHECK(!api_key.empty());
  GURL formatted_url = Client::FormatUrl(url, api_key);

  auto connection_factory = std::make_unique<ConnectionFactoryImpl>(
      formatted_url, network_context, logger, oak_session_driver,
      network_driver, channel);

  if (use_token_attestation) {
    connection_factory->EnableTokenAttestation(token_manager);
  }

  if (!proxy_url_string.empty()) {
    GURL proxy_url(proxy_url_string);
    if (!proxy_url.SchemeIsHTTPOrHTTPS()) {
      proxy_url = GURL(base::StrCat({"https://", proxy_url_string}));
    }
    if (proxy_url.SchemeIs(url::kHttpScheme)) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpsScheme);
      proxy_url = proxy_url.ReplaceComponents(replacements);
    }
    connection_factory->EnableProxy(proxy_url);
  }

  return base::WrapUnique(
      new ClientImpl(std::move(connection_factory), logger));
}

// static
GURL Client::FormatUrl(const std::string& url) {
  GURL base_url(url);
  if (base_url.is_valid()) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kWssScheme);
    base_url = base_url.ReplaceComponents(replacements);
  } else {
    base_url = GURL(base::StrCat({"wss://", url}));
  }
  return base_url;
}

// static
GURL Client::FormatUrl(const std::string& url, const std::string& api_key) {
  GURL formatted_url = FormatUrl(url);
  return net::AppendOrReplaceQueryParameter(formatted_url, "key", api_key);
}

}  // namespace private_ai
