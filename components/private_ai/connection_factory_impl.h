// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_FACTORY_IMPL_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_FACTORY_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/private_ai/connection_factory.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
class NetworkService;
}  // namespace network::mojom

namespace private_ai {

class LegionLogger;

namespace phosphor {
class TokenManager;
}

class ConnectionFactoryImpl : public ConnectionFactory {
 public:
  ConnectionFactoryImpl(const GURL& url,
                        network::mojom::NetworkContext* network_context,
                        LegionLogger* logger);
  ~ConnectionFactoryImpl() override;

  ConnectionFactoryImpl(const ConnectionFactoryImpl&) = delete;
  ConnectionFactoryImpl& operator=(const ConnectionFactoryImpl&) = delete;

  void EnableTokenAttestation(phosphor::TokenManager* token_manager);
  void EnableProxy(const GURL& proxy_url,
                   network::mojom::NetworkService* network_service);

  // ConnectionFactory override:
  std::unique_ptr<Connection> Create(
      base::RepeatingClosure on_disconnect) override;

 private:
  const GURL url_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const raw_ptr<LegionLogger> logger_;

  raw_ptr<phosphor::TokenManager> token_manager_ = nullptr;
  GURL proxy_url_;
  raw_ptr<network::mojom::NetworkService> network_service_ = nullptr;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_FACTORY_IMPL_H_
