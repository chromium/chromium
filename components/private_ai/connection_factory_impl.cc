// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_factory_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection_basic.h"
#include "components/private_ai/connection_factory.h"
#include "components/private_ai/connection_metrics.h"
#include "components/private_ai/connection_proxy.h"
#include "components/private_ai/connection_timeout.h"
#include "components/private_ai/connection_token_attestation.h"
#include "components/private_ai/connection_unused_timeout.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/secure_channel.h"
#include "components/private_ai/secure_channel_impl.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

namespace {

void RecordConnectionEstablishmentLatency(base::TimeTicks start_time) {
  base::UmaHistogramMediumTimes(
      "PrivateAi.SecureChannel.ConnectionEstablishmentLatency.Success",
      base::TimeTicks::Now() - start_time);
}

std::unique_ptr<Connection> CreateConnectionStack(
    const GURL& url,
    proto::FeatureName feature_name,
    PrivateAiLogger* logger,
    phosphor::TokenManager* token_manager,
    ConnectionFactoryImpl::SecureChannelFactoryOverride secure_channel_override,
    PrivateAiOakSessionDriver* oak_session_driver,
    base::RepeatingCallback<void(StatusCode)> on_disconnect,
    base::OnceClosure on_established,
    network::mojom::NetworkContext* network_context) {
  std::unique_ptr<SecureChannel::Factory> secure_channel_factory;
  if (secure_channel_override) {
    secure_channel_factory = secure_channel_override.Run();
  } else {
    secure_channel_factory = std::make_unique<SecureChannelImpl::FactoryImpl>(
        url, network_context, logger, oak_session_driver);
  }

  std::unique_ptr<Connection> connection = std::make_unique<ConnectionBasic>(
      std::move(secure_channel_factory), std::move(on_established),
      on_disconnect);

  if (token_manager) {
    connection = std::make_unique<ConnectionTokenAttestation>(
        std::move(connection), feature_name, token_manager, logger,
        on_disconnect);
  }

  return connection;
}

}  // namespace

ConnectionFactoryImpl::ConnectionFactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    PrivateAiLogger* logger,
    PrivateAiOakSessionDriver* oak_session_driver,
    PrivateAiNetworkDriver* network_driver)
    : url_(url),
      network_context_(network_context),
      logger_(logger),
      oak_session_driver_(oak_session_driver),
      network_driver_(network_driver) {
  CHECK(network_context_);
  CHECK(logger_);
  CHECK(oak_session_driver_);
  CHECK(network_driver_);

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

void ConnectionFactoryImpl::EnableProxy(const GURL& proxy_url) {
  proxy_url_ = proxy_url;
}

std::unique_ptr<Connection> ConnectionFactoryImpl::Create(
    proto::FeatureName feature_name,
    base::RepeatingCallback<void(StatusCode)> on_disconnect) {
  auto on_established = base::BindOnce(&RecordConnectionEstablishmentLatency,
                                       base::TimeTicks::Now());

  std::unique_ptr<Connection> connection;
  if (!proxy_url_.is_valid()) {
    logger_->LogInfo(FROM_HERE,
                     "Creating connection to Private AI server (direct).");
    connection = CreateConnectionStack(
        url_, feature_name, logger_, token_manager_, secure_channel_override_,
        oak_session_driver_, on_disconnect, std::move(on_established),
        network_context_);
  } else {
    logger_->LogInfo(FROM_HERE,
                     "Creating connection to Private AI server via proxy: " +
                         proxy_url_.spec());
    CHECK(token_manager_);
    // ConnectionProxy requires an inner factory that creates a connection
    // with token attestation.
    auto inner_connection_factory = base::BindOnce(
        &CreateConnectionStack, url_, feature_name, logger_, token_manager_,
        secure_channel_override_, oak_session_driver_, on_disconnect,
        std::move(on_established));

    connection = std::make_unique<ConnectionProxy>(
        proxy_url_, logger_, token_manager_, network_driver_,
        std::move(inner_connection_factory), on_disconnect);
  }

  connection = std::make_unique<ConnectionUnusedTimeout>(
      std::move(connection), on_disconnect,
      kPrivateAiUnusedConnectionTimeout.Get());

  connection = std::make_unique<ConnectionTimeout>(std::move(connection));

  connection = std::make_unique<ConnectionMetrics>(std::move(connection));

  return connection;
}

}  // namespace private_ai
