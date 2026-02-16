// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CONNECTION_FACTORY_IMPL_H_
#define COMPONENTS_LEGION_CONNECTION_FACTORY_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/legion/connection_factory.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
class NetworkService;
}

namespace private_ai {

class LegionLogger;

namespace phosphor {
class TokenManager;
}

// Factory for creating `Connection` instances that use API Key for client
// attestation.
class ApiKeyConnectionFactoryImpl : public ConnectionFactory {
 public:
  ApiKeyConnectionFactoryImpl(const GURL& url,
                              network::mojom::NetworkContext* network_context,
                              LegionLogger* logger);
  ~ApiKeyConnectionFactoryImpl() override;

  ApiKeyConnectionFactoryImpl(const ApiKeyConnectionFactoryImpl&) = delete;
  ApiKeyConnectionFactoryImpl& operator=(const ApiKeyConnectionFactoryImpl&) =
      delete;

  // ConnectionFactory override:
  std::unique_ptr<Connection> Create(
      base::RepeatingClosure on_disconnect) override;

 private:
  const GURL url_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const raw_ptr<LegionLogger> logger_;
};

// Factory for creating `Connection` instances that use blind token for client
// attestation.
//
// `url` should not contain API Key, otherwise it will lead to crash.
class TokenConnectionFactoryImpl : public ConnectionFactory {
 public:
  TokenConnectionFactoryImpl(const GURL& url,
                             network::mojom::NetworkContext* network_context,
                             phosphor::TokenManager* token_manager,
                             LegionLogger* logger);
  ~TokenConnectionFactoryImpl() override;

  TokenConnectionFactoryImpl(const TokenConnectionFactoryImpl&) = delete;
  TokenConnectionFactoryImpl& operator=(const TokenConnectionFactoryImpl&) =
      delete;

  // ConnectionFactory override:
  std::unique_ptr<Connection> Create(
      base::RepeatingClosure on_disconnect) override;

 private:
  const GURL url_;
  const raw_ptr<phosphor::TokenManager> token_manager_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const raw_ptr<LegionLogger> logger_;
};

// Factory for creating `Connection` instances that use blind token for client
// attestation and proxy connection through a privacy proxy.
class ProxyWithTokenConnectionFactoryImpl : public ConnectionFactory {
 public:
  ProxyWithTokenConnectionFactoryImpl(
      const GURL& url,
      const GURL& proxy_url,
      network::mojom::NetworkService* network_service,
      phosphor::TokenManager* token_manager,
      LegionLogger* logger);
  ~ProxyWithTokenConnectionFactoryImpl() override;

  ProxyWithTokenConnectionFactoryImpl(
      const ProxyWithTokenConnectionFactoryImpl&) = delete;
  ProxyWithTokenConnectionFactoryImpl& operator=(
      const ProxyWithTokenConnectionFactoryImpl&) = delete;

  // ConnectionFactory override:
  std::unique_ptr<Connection> Create(
      base::RepeatingClosure on_disconnect) override;

 private:
  const GURL url_;
  const GURL proxy_url_;
  const raw_ptr<network::mojom::NetworkService> network_service_;
  const raw_ptr<phosphor::TokenManager> token_manager_;
  const raw_ptr<LegionLogger> logger_;
};

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_CONNECTION_FACTORY_IMPL_H_
