// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "components/legion/client_impl.h"
#include "components/legion/common/legion_logger.h"
#include "components/legion/connection_factory_impl.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/token_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace legion {

// static
std::unique_ptr<Client> Client::CreateWithApiKey(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    std::unique_ptr<LegionLogger> logger) {
  CHECK(base::FeatureList::IsEnabled(kLegion));

  auto connection_factory_impl = std::make_unique<ApiKeyConnectionFactoryImpl>(
      url, network_context, logger.get());

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(
      new ClientImpl(std::move(connection_factory_impl), std::move(logger)));
}

// static
std::unique_ptr<Client> Client::CreateWithToken(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager,
    std::unique_ptr<LegionLogger> logger) {
  CHECK(base::FeatureList::IsEnabled(kLegion));
  CHECK(network_context);

  auto connection_factory_impl = std::make_unique<TokenConnectionFactoryImpl>(
      url, network_context, token_manager, logger.get());

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(
      new ClientImpl(std::move(connection_factory_impl), std::move(logger)));
}

// static
std::unique_ptr<Client> Client::CreateWithProxyAndToken(
    const GURL& url,
    const GURL& proxy_url,
    network::mojom::NetworkService* network_service,
    phosphor::TokenManager* token_manager,
    std::unique_ptr<LegionLogger> logger) {
  CHECK(base::FeatureList::IsEnabled(kLegion));

  auto connection_factory_impl =
      std::make_unique<ProxyWithTokenConnectionFactoryImpl>(
          url, proxy_url, network_service, token_manager, logger.get());

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(
      new ClientImpl(std::move(connection_factory_impl), std::move(logger)));
}

// static
std::unique_ptr<Client> Client::Create(
    const std::string& url,
    const std::string& api_key,
    const std::string& proxy_url_string,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager,
    network::mojom::NetworkService* network_service,
    std::unique_ptr<LegionLogger> logger) {
  if (!api_key.empty()) {
    return Client::CreateWithApiKey(Client::FormatUrl(url, api_key),
                                    network_context, std::move(logger));
  }

  GURL formatted_url = Client::FormatUrl(url);
  if (!proxy_url_string.empty()) {
    GURL proxy_url(proxy_url_string);
    if (!proxy_url.SchemeIsHTTPOrHTTPS()) {
      proxy_url = GURL(base::StrCat({"https://", proxy_url_string}));
    }
    return Client::CreateWithProxyAndToken(formatted_url, proxy_url,
                                           network_service, token_manager,
                                           std::move(logger));
  }

  return Client::CreateWithToken(formatted_url, network_context, token_manager,
                                 std::move(logger));
}

// static
GURL Client::FormatUrl(const std::string& url) {
  return GURL(base::StrCat({"wss://", url}));
}

// static
GURL Client::FormatUrl(const std::string& url, const std::string& api_key) {
  return GURL(base::StrCat({"wss://", url, "?key=", api_key}));
}

}  // namespace legion
