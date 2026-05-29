// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_PRIVATE_AI_NETWORK_DRIVER_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_PRIVATE_AI_NETWORK_DRIVER_H_

#include "components/private_ai/private_ai_network_driver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

class FakePrivateAiNetworkDriver : public PrivateAiNetworkDriver {
 public:
  FakePrivateAiNetworkDriver();
  ~FakePrivateAiNetworkDriver() override;

  network::mojom::CertVerifierServiceRemoteParamsPtr GetCertVerifierParams()
      override;

  void CreateNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
      network::mojom::NetworkContextParamsPtr params) override;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_PRIVATE_AI_NETWORK_DRIVER_H_
