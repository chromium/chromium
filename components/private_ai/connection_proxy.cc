// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_proxy.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "net/http/http_request_headers.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom.h"

namespace private_ai {

namespace {

network::mojom::CustomProxyConfigPtr CreateCustomProxyConfig(
    const GURL& proxy_url,
    const std::string& auth_token) {
  network::mojom::CustomProxyConfigPtr proxy_config =
      network::mojom::CustomProxyConfig::New();
  proxy_config->rules.ParseFromString(proxy_url.spec());
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                    base::StrCat({"PrivateToken ", auth_token}));
  proxy_config->connect_tunnel_headers = headers;
  proxy_config->should_override_existing_config = true;
  return proxy_config;
}

}  // namespace

ConnectionProxy::PendingRequest::PendingRequest(proto::PrivateAiRequest request,
                                                base::TimeDelta timeout,
                                                OnRequestCallback callback)
    : request(std::move(request)),
      timeout(timeout),
      callback(std::move(callback)) {}

ConnectionProxy::PendingRequest::~PendingRequest() = default;

ConnectionProxy::PendingRequest::PendingRequest(PendingRequest&&) = default;

ConnectionProxy::PendingRequest& ConnectionProxy::PendingRequest::operator=(
    PendingRequest&&) = default;

ConnectionProxy::ConnectionProxy(
    const GURL& proxy_url,
    phosphor::TokenManager* token_manager,
    network::mojom::NetworkService* network_service,
    InnerConnectionFactory inner_connection_factory,
    base::OnceCallback<void(ErrorCode)> on_disconnect)
    : proxy_url_(proxy_url),
      token_manager_(token_manager),
      network_service_(network_service),
      inner_connection_factory_(std::move(inner_connection_factory)),
      on_disconnect_(std::move(on_disconnect)) {
  CHECK(proxy_url_.is_valid());
  CHECK(token_manager_);
  CHECK(network_service_);
  CHECK(inner_connection_factory_);
  CHECK(on_disconnect_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ConnectionProxy::FetchToken, weak_factory_.GetWeakPtr()));
}

ConnectionProxy::~ConnectionProxy() = default;

void ConnectionProxy::Send(proto::PrivateAiRequest request,
                           base::TimeDelta timeout,
                           OnRequestCallback callback) {
  if (is_initializing_) {
    pending_requests_.emplace_back(std::move(request), timeout,
                                   std::move(callback));
    return;
  }

  if (!inner_connection_) {
    // Initialization failed or connection disconnected.
    std::move(callback).Run(base::unexpected(ErrorCode::kError));
    return;
  }

  inner_connection_->Send(std::move(request), timeout, std::move(callback));
}

void ConnectionProxy::OnDestroy(ErrorCode error) {
  on_disconnect_.Reset();

  auto pending_requests = std::move(pending_requests_);
  for (auto& pending : pending_requests) {
    std::move(pending.callback).Run(base::unexpected(error));
  }

  if (inner_connection_) {
    inner_connection_->OnDestroy(error);
  }

  token_manager_ = nullptr;
  network_service_ = nullptr;
  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionProxy::CallOnDisconnect(ErrorCode error_code) {
  if (on_disconnect_) {
    std::move(on_disconnect_).Run(error_code);
  }
}

void ConnectionProxy::FetchToken() {
  token_manager_->GetAuthTokenForProxy(base::BindOnce(
      &ConnectionProxy::OnProxyToken, weak_factory_.GetWeakPtr()));
}

void ConnectionProxy::OnProxyToken(
    std::optional<phosphor::BlindSignedAuthToken> auth_token) {
  is_initializing_ = false;

  if (!auth_token) {
    LOG(ERROR) << "Failed to get auth token for proxy.";
    CallOnDisconnect(ErrorCode::kError);
    return;
  }

  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->initial_custom_proxy_config =
      CreateCustomProxyConfig(proxy_url_, auth_token->token);

  network_service_->CreateNetworkContext(
      proxied_context_.BindNewPipeAndPassReceiver(), std::move(context_params));

  inner_connection_ =
      std::move(inner_connection_factory_).Run(proxied_context_.get());

  for (auto& pending : pending_requests_) {
    inner_connection_->Send(std::move(pending.request), pending.timeout,
                            std::move(pending.callback));
  }
  pending_requests_.clear();
}

}  // namespace private_ai
