// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/connection_factory_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/legion/attestation/handler_impl.h"
#include "components/legion/connection_basic.h"
#include "components/legion/connection_metrics.h"
#include "components/legion/connection_timeout.h"
#include "components/legion/connection_token_attestation.h"
#include "components/legion/secure_channel_impl.h"
#include "components/legion/secure_session_async_impl.h"
#include "components/legion/websocket_client.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace legion {

namespace {

std::unique_ptr<SecureChannel> CreateSecureChannel(
    const GURL& url,
    network::mojom::NetworkContext* network_context) {
  auto transport = std::make_unique<WebSocketClient>(url, network_context);
  auto secure_session = std::make_unique<SecureSessionAsyncImpl>();
  auto attestation_handler = std::make_unique<AttestationHandlerImpl>();

  return std::make_unique<SecureChannelImpl>(std::move(transport),
                                             std::move(secure_session),
                                             std::move(attestation_handler));
}

// Creates connection composition: Timeout(Metrics(Basic)).
std::unique_ptr<Connection> CreateBasicMetricsTimeoutConnection(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    base::OnceClosure on_disconnect) {
  auto connection_basic = std::make_unique<ConnectionBasic>(
      CreateSecureChannel(url, network_context), std::move(on_disconnect));

  auto connection_metrics =
      std::make_unique<ConnectionMetrics>(std::move(connection_basic));

  auto connection_timeout =
      std::make_unique<ConnectionTimeout>(std::move(connection_metrics));

  return connection_timeout;
}

}  // namespace

ApiKeyConnectionFactoryImpl::ApiKeyConnectionFactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context)
    : url_(url), network_context_(network_context) {
  CHECK(network_context_);

  std::string api_key;
  CHECK(net::GetValueForKeyInQuery(url, "key", &api_key))
      << "API key must be passed in the URL for ApiKeyConnectionFactoryImpl";
  CHECK(!api_key.empty());
}

ApiKeyConnectionFactoryImpl::~ApiKeyConnectionFactoryImpl() = default;

std::unique_ptr<Connection> ApiKeyConnectionFactoryImpl::Create(
    base::RepeatingClosure on_disconnect) {
  return CreateBasicMetricsTimeoutConnection(url_, network_context_,
                                             std::move(on_disconnect));
}

TokenConnectionFactoryImpl::TokenConnectionFactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager)
    : url_(url),
      network_context_(network_context),
      token_manager_(token_manager) {
  CHECK(network_context_);
  CHECK(token_manager_);

  std::string api_key;
  CHECK(!net::GetValueForKeyInQuery(url, "key", &api_key))
      << "API key must NOT be passed in the URL for TokenConnectionFactoryImpl";
}

TokenConnectionFactoryImpl::~TokenConnectionFactoryImpl() = default;

std::unique_ptr<Connection> TokenConnectionFactoryImpl::Create(
    base::RepeatingClosure on_disconnect) {
  auto connection_timeout = CreateBasicMetricsTimeoutConnection(
      url_, network_context_, on_disconnect);

  auto connection_token_attestation =
      std::make_unique<ConnectionTokenAttestation>(
          std::move(connection_timeout), token_manager_,
          std::move(on_disconnect));

  return connection_token_attestation;
}

}  // namespace legion
