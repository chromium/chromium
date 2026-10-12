// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_IOS_PRIVATE_AI_NETWORK_DRIVER_IOS_H_
#define COMPONENTS_PRIVATE_AI_IOS_PRIVATE_AI_NETWORK_DRIVER_IOS_H_

#include "components/private_ai/private_ai_network_driver.h"

namespace private_ai {

// iOS implementation of the Private AI Network Driver.
class PrivateAiNetworkDriverIOS : public PrivateAiNetworkDriver {
 public:
  PrivateAiNetworkDriverIOS();

  ~PrivateAiNetworkDriverIOS() override;

  PrivateAiNetworkDriverIOS(const PrivateAiNetworkDriverIOS&) = delete;
  PrivateAiNetworkDriverIOS& operator=(const PrivateAiNetworkDriverIOS&) =
      delete;

  // private_ai::PrivateAiNetworkDriver:
  network::mojom::CertVerifierServiceRemoteParamsPtr GetCertVerifierParams()
      override;

  void CreateNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
      network::mojom::NetworkContextParamsPtr params) override;

  std::optional<bool> IsProxyConfigured() const override;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_IOS_PRIVATE_AI_NETWORK_DRIVER_IOS_H_
