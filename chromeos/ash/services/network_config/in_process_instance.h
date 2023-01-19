// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_IN_PROCESS_INSTANCE_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_IN_PROCESS_INSTANCE_H_

#include "base/component_export.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::network_config {

COMPONENT_EXPORT(IN_PROCESS_NETWORK_CONFIG)
void BindToInProcessInstance(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver);

COMPONENT_EXPORT(IN_PROCESS_NETWORK_CONFIG)
void OverrideInProcessInstanceForTesting(
    chromeos::network_config::mojom::CrosNetworkConfig* network_config);

}  // namespace ash::network_config

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_IN_PROCESS_INSTANCE_H_
