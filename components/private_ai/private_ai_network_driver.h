// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PRIVATE_AI_NETWORK_DRIVER_H_
#define COMPONENTS_PRIVATE_AI_PRIVATE_AI_NETWORK_DRIVER_H_

#include <optional>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

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

  // Returns whether an explicit proxy (manual proxy rules, PAC script URL,
  // proxy override rules, or dynamic routing rules) is configured for Chrome's
  // profile, false if direct (or only WPAD auto-detect), or std::nullopt if
  // unknown/unsupported on the current platform.
  virtual std::optional<bool> IsProxyConfigured() const = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PRIVATE_AI_NETWORK_DRIVER_H_
