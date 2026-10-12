// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/content/private_ai_network_driver_content.h"

#include <optional>
#include <utility>

#include "content/public/browser/network_service_instance.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

PrivateAiNetworkDriverContent::PrivateAiNetworkDriverContent(
    base::RepeatingCallback<std::optional<bool>()> is_proxy_configured_callback)
    : is_proxy_configured_callback_(std::move(is_proxy_configured_callback)) {}

PrivateAiNetworkDriverContent::~PrivateAiNetworkDriverContent() = default;

network::mojom::CertVerifierServiceRemoteParamsPtr
PrivateAiNetworkDriverContent::GetCertVerifierParams() {
  return content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
}

void PrivateAiNetworkDriverContent::CreateNetworkContext(
    mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
    network::mojom::NetworkContextParamsPtr params) {
  content::CreateNetworkContextInNetworkService(std::move(receiver),
                                                std::move(params));
}

std::optional<bool> PrivateAiNetworkDriverContent::IsProxyConfigured() const {
  if (!is_proxy_configured_callback_) {
    return std::nullopt;
  }
  return is_proxy_configured_callback_.Run();
}

}  // namespace private_ai
