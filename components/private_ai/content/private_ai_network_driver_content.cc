// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/content/private_ai_network_driver_content.h"

#include "content/public/browser/network_service_instance.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace private_ai {

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

}  // namespace private_ai
