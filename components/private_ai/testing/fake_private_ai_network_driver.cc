// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/fake_private_ai_network_driver.h"

namespace private_ai {

FakePrivateAiNetworkDriver::FakePrivateAiNetworkDriver() = default;

FakePrivateAiNetworkDriver::~FakePrivateAiNetworkDriver() = default;

network::mojom::CertVerifierServiceRemoteParamsPtr
FakePrivateAiNetworkDriver::GetCertVerifierParams() {
  mojo::PendingRemote<cert_verifier::mojom::CertVerifierService> remote;
  mojo::PendingReceiver<cert_verifier::mojom::CertVerifierServiceClient> client;
  return network::mojom::CertVerifierServiceRemoteParams::New(
      std::move(remote), std::move(client));
}

void FakePrivateAiNetworkDriver::CreateNetworkContext(
    mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
    network::mojom::NetworkContextParamsPtr params) {}

}  // namespace private_ai
