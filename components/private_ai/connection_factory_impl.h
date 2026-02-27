// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_FACTORY_IMPL_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_FACTORY_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/private_ai/connection_factory.h"
#include "components/private_ai/secure_channel.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
class NetworkService;
}  // namespace network::mojom

namespace private_ai {

class PrivateAiLogger;

namespace phosphor {
class TokenManager;
}

class ConnectionFactoryImpl : public ConnectionFactory {
 public:
  using SecureChannelFactoryOverride =
      base::RepeatingCallback<std::unique_ptr<SecureChannel::Factory>()>;

  ConnectionFactoryImpl(const GURL& url,
                        network::mojom::NetworkContext* network_context,
                        PrivateAiLogger* logger);
  ~ConnectionFactoryImpl() override;

  ConnectionFactoryImpl(const ConnectionFactoryImpl&) = delete;
  ConnectionFactoryImpl& operator=(const ConnectionFactoryImpl&) = delete;

  void EnableTokenAttestation(phosphor::TokenManager* token_manager);
  void EnableProxy(const GURL& proxy_url,
                   network::mojom::NetworkService* network_service);

  void SetSecureChannelFactoryForTesting(
      SecureChannelFactoryOverride override) {
    secure_channel_override_ = std::move(override);
  }

  // ConnectionFactory override:
  std::unique_ptr<Connection> Create(
      base::OnceCallback<void(ErrorCode)> on_disconnect) override;

 private:
  const GURL url_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const raw_ptr<PrivateAiLogger> logger_;

  SecureChannelFactoryOverride secure_channel_override_;

  raw_ptr<phosphor::TokenManager> token_manager_ = nullptr;
  GURL proxy_url_;
  raw_ptr<network::mojom::NetworkService> network_service_ = nullptr;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_FACTORY_IMPL_H_
