// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_PROXY_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_PROXY_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkService;
}  // namespace network::mojom

namespace private_ai {

namespace phosphor {
class TokenManager;
}

// A decorator for `Connection` that buffers requests until a proxy connection
// can be established.
//
// It fetches a proxy token from `TokenManager`, creates a `NetworkContext`
// with proxy configuration using `NetworkContextFactory`, and then creates
// the inner connection using the provided `InnerConnectionFactory`.
class ConnectionProxy : public Connection {
 public:
  // Factory for creating the inner connection given a NetworkContext.
  using InnerConnectionFactory = base::OnceCallback<std::unique_ptr<Connection>(
      network::mojom::NetworkContext*)>;

  ConnectionProxy(const GURL& proxy_url,
                  phosphor::TokenManager* token_manager,
                  network::mojom::NetworkService* network_service,
                  InnerConnectionFactory inner_connection_factory,
                  base::OnceCallback<void(ErrorCode)> on_disconnect);
  ~ConnectionProxy() override;

  ConnectionProxy(const ConnectionProxy&) = delete;
  ConnectionProxy& operator=(const ConnectionProxy&) = delete;

  // Connection override:
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(ErrorCode error) override;

 private:
  struct PendingRequest {
    PendingRequest(proto::PrivateAiRequest request,
                   base::TimeDelta timeout,
                   OnRequestCallback callback);
    ~PendingRequest();

    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    proto::PrivateAiRequest request;
    base::TimeDelta timeout;
    OnRequestCallback callback;
  };

  void FetchToken();
  void OnProxyToken(std::optional<phosphor::BlindSignedAuthToken> auth_token);
  void CallOnDisconnect(ErrorCode error_code);

  const GURL proxy_url_;
  raw_ptr<phosphor::TokenManager> token_manager_;
  raw_ptr<network::mojom::NetworkService> network_service_;
  InnerConnectionFactory inner_connection_factory_;

  // Called to trigger a disconnect and destruction of the connection.
  base::OnceCallback<void(ErrorCode)> on_disconnect_;

  mojo::Remote<network::mojom::NetworkContext> proxied_context_;
  std::unique_ptr<Connection> inner_connection_;

  std::vector<PendingRequest> pending_requests_;
  bool is_initializing_ = true;

  base::WeakPtrFactory<ConnectionProxy> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_PROXY_H_
