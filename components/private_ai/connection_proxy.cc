// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_proxy.h"

#include <utility>

#include "base/base64url.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/common/base64_utils.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/proxy_config.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "url/origin.h"

namespace private_ai {

namespace internal {

network::mojom::CustomProxyConfigPtr CreateCustomProxyConfig(
    const GURL& proxy_url,
    const phosphor::BlindSignedAuthToken& auth_token,
    PrivateAiLogger* logger) {
  network::mojom::CustomProxyConfigPtr proxy_config =
      network::mojom::CustomProxyConfig::New();

  // The proxy rule parser expects exactly "[scheme://]host[:port]" and fails or
  // ignores if a trailing slash is present like using GURL::spec().
  proxy_config->rules.ParseFromString(
      url::Origin::Create(proxy_url).Serialize());

  std::optional<std::string> formatted_token = ConvertBase64toBase64Url(
      auth_token.token, base::Base64UrlEncodePolicy::OMIT_PADDING);
  if (!formatted_token) {
    logger->LogError(FROM_HERE, "Invalid base64 encoding in private token.");
    return nullptr;
  }

  std::optional<std::string> formatted_extensions = ConvertBase64toBase64Url(
      auth_token.encoded_extensions, base::Base64UrlEncodePolicy::OMIT_PADDING);
  if (!formatted_extensions) {
    logger->LogError(FROM_HERE,
                     "Invalid base64 encoding in private token extensions.");
    return nullptr;
  }

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"PrivateToken token=\"", *formatted_token,
                    "\" extensions=\"", *formatted_extensions, "\""}));
  proxy_config->connect_tunnel_headers = headers;
  proxy_config->should_override_existing_config = true;
  proxy_config->allow_non_idempotent_methods = true;
  return proxy_config;
}

}  // namespace internal

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
    PrivateAiLogger* logger,
    phosphor::TokenManager* token_manager,
    InnerConnectionFactory inner_connection_factory,
    base::OnceCallback<void(StatusCode)> on_disconnect)
    : proxy_url_(proxy_url),
      logger_(logger),
      token_manager_(token_manager),
      inner_connection_factory_(std::move(inner_connection_factory)),
      on_disconnect_(std::move(on_disconnect)) {
  CHECK(proxy_url_.is_valid());
  CHECK(logger_);
  CHECK(token_manager_);
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
    std::move(callback).Run(base::unexpected(StatusCode::kError));
    return;
  }

  inner_connection_->Send(std::move(request), timeout, std::move(callback));
}

void ConnectionProxy::OnDestroy(StatusCode status_code) {
  on_disconnect_.Reset();

  auto pending_requests = std::move(pending_requests_);
  for (auto& pending : pending_requests) {
    std::move(pending.callback).Run(base::unexpected(status_code));
  }

  if (inner_connection_) {
    inner_connection_->OnDestroy(status_code);
  }

  token_manager_ = nullptr;
  logger_ = nullptr;
  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionProxy::CallOnDisconnect(StatusCode status_code) {
  if (on_disconnect_) {
    std::move(on_disconnect_).Run(status_code);
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
    logger_->LogError(FROM_HERE, "Failed to get auth token for proxy.");
    CallOnDisconnect(StatusCode::kError);
    return;
  }


  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->enforce_chrome_ct_policy = true;
  context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->initial_custom_proxy_config =
      internal::CreateCustomProxyConfig(proxy_url_, *auth_token, logger_);

  if (!context_params->initial_custom_proxy_config) {
    CallOnDisconnect(StatusCode::kProxyConfigFailed);
    return;
  }

  logger_->LogInfo(FROM_HERE, "Got auth token for proxy. Connecting to " +
                                  proxy_url_.spec());

  content::CreateNetworkContextInNetworkService(
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
