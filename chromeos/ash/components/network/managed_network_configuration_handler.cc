// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_network_configuration_handler.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

ManagedNetworkConfigurationHandler::~ManagedNetworkConfigurationHandler() =
    default;

// static
std::unique_ptr<ManagedNetworkConfigurationHandler>
ManagedNetworkConfigurationHandler::InitializeForTesting(
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkDeviceHandler* network_device_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    UIProxyConfigService* ui_proxy_config_service) {
  auto* handler = new ManagedNetworkConfigurationHandlerImpl();
  handler->Init(/*cellular_policy_handler=*/nullptr,
                /*managed_cellular_pref_handler=*/nullptr,
                network_state_handler, network_profile_handler,
                network_configuration_handler, network_device_handler,
                /*prohibited_technologies_handler=*/nullptr,
                /*hotspot_controller=*/nullptr);
  handler->set_ui_proxy_config_service(ui_proxy_config_service);
  return base::WrapUnique(handler);
}

}  // namespace ash
