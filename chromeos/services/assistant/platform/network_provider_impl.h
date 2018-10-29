// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_NETWORK_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_NETWORK_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "libassistant/shared/public/platform_net.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace chromeos {
namespace assistant {

class NetworkProviderImpl
    : public assistant_client::NetworkProvider,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit NetworkProviderImpl(
      network::NetworkConnectionTracker* network_connection_tracker);
  ~NetworkProviderImpl() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // assistant_client::NetworkProvider::NetworkChangeObserver overrides:
  ConnectionStatus GetConnectionStatus() override;
  assistant_client::MdnsResponder* GetMdnsResponder() override;

 private:
  network::NetworkConnectionTracker* network_connection_tracker_;
  network::mojom::ConnectionType connection_type_;
  base::WeakPtrFactory<NetworkProviderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_NETWORK_PROVIDER_IMPL_H_
