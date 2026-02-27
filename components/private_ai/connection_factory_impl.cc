// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_factory_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection_basic.h"
#include "components/private_ai/connection_factory.h"
#include "components/private_ai/connection_metrics.h"
#include "components/private_ai/connection_proxy.h"
#include "components/private_ai/connection_timeout.h"
#include "components/private_ai/connection_token_attestation.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/secure_channel.h"
#include "components/private_ai/secure_channel_impl.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace private_ai {

namespace {

std::unique_ptr<Connection> CreateConnectionStack(
    const GURL& url,
    PrivateAiLogger* logger,
    phosphor::TokenManager* token_manager,
    std::unique_ptr<SecureChannel::Factory> secure_channel_factory,
    base::OnceCallback<void(ErrorCode)> on_disconnect,
    network::mojom::NetworkContext* network_context) {
  auto split_on_disconnect = base::SplitOnceCallback(std::move(on_disconnect));

  std::unique_ptr<Connection> connection = std::make_unique<ConnectionBasic>(
      std::move(secure_channel_factory), std::move(split_on_disconnect.first));

  connection = std::make_unique<ConnectionMetrics>(std::move(connection));

  if (token_manager) {
    connection = std::make_unique<ConnectionTokenAttestation>(
        std::move(connection), token_manager, logger,
        std::move(split_on_disconnect.second));
  }

  return connection;
}

}  // namespace

ConnectionFactoryImpl::ConnectionFactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    PrivateAiLogger* logger)
    : url_(url), network_context_(network_context), logger_(logger) {
  CHECK(network_context_);
  CHECK(logger_);

  std::string api_key;
  CHECK(net::GetValueForKeyInQuery(url, "key", &api_key))
      << "API key must be passed in the URL for ConnectionFactoryImpl";
  CHECK(!api_key.empty());
}

ConnectionFactoryImpl::~ConnectionFactoryImpl() = default;

void ConnectionFactoryImpl::EnableTokenAttestation(
    phosphor::TokenManager* token_manager) {
  token_manager_ = token_manager;
}

void ConnectionFactoryImpl::EnableProxy(
    const GURL& proxy_url,
    network::mojom::NetworkService* network_service) {
  proxy_url_ = proxy_url;
  network_service_ = network_service;
}

std::unique_ptr<Connection> ConnectionFactoryImpl::Create(
    base::OnceCallback<void(ErrorCode)> on_disconnect) {
  std::unique_ptr<SecureChannel::Factory> secure_channel_factory;
  if (secure_channel_override_) {
    secure_channel_factory = secure_channel_override_.Run();
  } else {
    secure_channel_factory = std::make_unique<SecureChannelImpl::FactoryImpl>(
        url_, network_context_, logger_);
  }

  std::unique_ptr<Connection> connection;
  if (!proxy_url_.is_valid()) {
    connection = CreateConnectionStack(
        url_, logger_, token_manager_, std::move(secure_channel_factory),
        std::move(on_disconnect), network_context_);
  } else {
    CHECK(network_service_);
    CHECK(token_manager_);
    auto split_on_disconnect =
        base::SplitOnceCallback(std::move(on_disconnect));
    // ConnectionProxy requires an inner factory that creates a connection
    // with token attestation.
    auto inner_connection_factory =
        base::BindOnce(&CreateConnectionStack, url_, logger_, token_manager_,
                       std::move(secure_channel_factory),
                       std::move(split_on_disconnect.first));

    connection = std::make_unique<ConnectionProxy>(
        proxy_url_, token_manager_, network_service_,
        std::move(inner_connection_factory),
        std::move(split_on_disconnect.second));
  }
  connection = std::make_unique<ConnectionTimeout>(std::move(connection));
  return connection;
}

}  // namespace private_ai
