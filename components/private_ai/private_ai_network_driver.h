// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PRIVATE_AI_NETWORK_DRIVER_H_
#define COMPONENTS_PRIVATE_AI_PRIVATE_AI_NETWORK_DRIVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

// Abstract interface for providing platform-specific implementations of
// network capabilities required by the Private AI component.
class PrivateAiNetworkDriver {
 public:
  virtual ~PrivateAiNetworkDriver() = default;

  // Returns params configured to initialize the network context.
  virtual network::mojom::CertVerifierServiceRemoteParamsPtr
  GetCertVerifierParams() = 0;

  // Creates a new NetworkContext based on params.
  virtual void CreateNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
      network::mojom::NetworkContextParamsPtr params) = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PRIVATE_AI_NETWORK_DRIVER_H_
