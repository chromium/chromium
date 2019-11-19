// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ManagedNetworkConfigurationHandler::~ManagedNetworkConfigurationHandler() =
    default;

// static
std::unique_ptr<ManagedNetworkConfigurationHandler>
ManagedNetworkConfigurationHandler::InitializeForTesting(
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkDeviceHandler* network_device_handler,
    NetworkConfigurationHandler* network_configuration_handler) {
  auto* handler = new ManagedNetworkConfigurationHandlerImpl();
  handler->Init(network_state_handler, network_profile_handler,
                network_configuration_handler, network_device_handler,
                /*prohibitied_technologies_handler=*/nullptr);
  return base::WrapUnique(handler);
}

}  // namespace chromeos
