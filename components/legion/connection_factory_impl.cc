// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/connection_factory_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/legion/common/legion_logger.h"
#include "components/legion/connection_basic.h"
#include "components/legion/connection_metrics.h"
#include "components/legion/connection_proxy.h"
#include "components/legion/connection_timeout.h"
#include "components/legion/connection_token_attestation.h"
#include "components/legion/phosphor/token_manager.h"
#include "components/legion/secure_channel_impl.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace private_ai {

namespace {

// Creates connection composition: Timeout(Metrics(Basic)).
std::unique_ptr<Connection> CreateBasicMetricsTimeoutConnection(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    LegionLogger* logger,
    base::OnceClosure on_disconnect) {
  auto connection_basic = std::make_unique<ConnectionBasic>(
      std::make_unique<SecureChannelImpl::FactoryImpl>(url, network_context,
                                                       logger),
      std::move(on_disconnect));

  auto connection_metrics =
      std::make_unique<ConnectionMetrics>(std::move(connection_basic));

  auto connection_timeout =
      std::make_unique<ConnectionTimeout>(std::move(connection_metrics));

  return connection_timeout;
}

std::unique_ptr<Connection> CreateTokenAttestationConnection(
    const GURL& url,
    phosphor::TokenManager* token_manager,
    LegionLogger* logger,
    base::RepeatingClosure on_disconnect,
    network::mojom::NetworkContext* network_context) {
  auto connection_timeout = CreateBasicMetricsTimeoutConnection(
      url, network_context, logger, on_disconnect);

  auto connection_token_attestation =
      std::make_unique<ConnectionTokenAttestation>(
          std::move(connection_timeout), token_manager,
          std::move(on_disconnect));

  return connection_token_attestation;
}

}  // namespace

ApiKeyConnectionFactoryImpl::ApiKeyConnectionFactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    LegionLogger* logger)
    : url_(url), network_context_(network_context), logger_(logger) {
  CHECK(network_context_);
  CHECK(logger_);

  std::string api_key;
  CHECK(net::GetValueForKeyInQuery(url, "key", &api_key))
      << "API key must be passed in the URL for ApiKeyConnectionFactoryImpl";
  CHECK(!api_key.empty());
}

ApiKeyConnectionFactoryImpl::~ApiKeyConnectionFactoryImpl() = default;

std::unique_ptr<Connection> ApiKeyConnectionFactoryImpl::Create(
    base::RepeatingClosure on_disconnect) {
  return CreateBasicMetricsTimeoutConnection(url_, network_context_, logger_,
                                             std::move(on_disconnect));
}

TokenConnectionFactoryImpl::TokenConnectionFactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager,
    LegionLogger* logger)
    : url_(url),
      token_manager_(token_manager),
      network_context_(network_context),
      logger_(logger) {
  CHECK(token_manager_);
  CHECK(network_context_);
  CHECK(logger_);

  std::string api_key;
  CHECK(!net::GetValueForKeyInQuery(url, "key", &api_key))
      << "API key must NOT be passed in the URL for TokenConnectionFactoryImpl";
}

TokenConnectionFactoryImpl::~TokenConnectionFactoryImpl() = default;

std::unique_ptr<Connection> TokenConnectionFactoryImpl::Create(
    base::RepeatingClosure on_disconnect) {
  return CreateTokenAttestationConnection(url_, token_manager_, logger_,
                                          std::move(on_disconnect),
                                          network_context_);
}

ProxyWithTokenConnectionFactoryImpl::ProxyWithTokenConnectionFactoryImpl(
    const GURL& url,
    const GURL& proxy_url,
    network::mojom::NetworkService* network_service,
    phosphor::TokenManager* token_manager,
    LegionLogger* logger)
    : url_(url),
      proxy_url_(proxy_url),
      network_service_(network_service),
      token_manager_(token_manager),
      logger_(logger) {
  CHECK(proxy_url_.is_valid());
  CHECK(token_manager_);
  CHECK(network_service_);
  CHECK(logger_);

  std::string api_key;
  CHECK(!net::GetValueForKeyInQuery(url, "key", &api_key))
      << "API key must NOT be passed in the URL for "
         "ProxyWithTokenConnectionFactoryImpl";
}

ProxyWithTokenConnectionFactoryImpl::~ProxyWithTokenConnectionFactoryImpl() =
    default;

std::unique_ptr<Connection> ProxyWithTokenConnectionFactoryImpl::Create(
    base::RepeatingClosure on_disconnect) {
  auto inner_connection_factory =
      base::BindOnce(&CreateTokenAttestationConnection, url_, token_manager_,
                     logger_, on_disconnect);

  return std::make_unique<ConnectionProxy>(
      proxy_url_, token_manager_.get(), network_service_,
      std::move(inner_connection_factory), std::move(on_disconnect));
}

}  // namespace private_ai
