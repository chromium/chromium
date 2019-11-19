// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/in_process_instance.h"

#include "base/no_destructor.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace network_config {

namespace {

mojom::CrosNetworkConfig* g_network_config_override;

mojo::ReceiverSet<mojom::CrosNetworkConfig>& GetOverrideReceivers() {
  static base::NoDestructor<mojo::ReceiverSet<mojom::CrosNetworkConfig>>
      receivers;
  return *receivers;
}

}  // namespace

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosNetworkConfig> receiver) {
  if (g_network_config_override) {
    GetOverrideReceivers().Add(g_network_config_override, std::move(receiver));
    return;
  }

  if (!NetworkHandler::IsInitialized()) {
    DVLOG(1) << "Ignoring request to bind Network Config service because no "
             << "NetworkHandler has been initialized.";
    return;
  }

  static base::NoDestructor<CrosNetworkConfig> instance;
  instance->BindReceiver(std::move(receiver));
}

void OverrideInProcessInstanceForTesting(
    mojom::CrosNetworkConfig* network_config) {
  g_network_config_override = network_config;

  // Wipe out the set of override receivers any time a new override is set.
  GetOverrideReceivers().Clear();
}

}  // namespace network_config
}  // namespace chromeos
