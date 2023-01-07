// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_state_test_helper.h"

#include "base/run_loop.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

NetworkStateTestHelper::NetworkStateTestHelper(
    bool use_default_devices_and_services) {
  AddDefaultProfiles();

  network_state_handler_ = NetworkStateHandler::InitializeForTest();
  network_device_handler_ =
      NetworkDeviceHandler::InitializeForTesting(network_state_handler_.get());

  if (!use_default_devices_and_services)
    ResetDevicesAndServices();
}

NetworkStateTestHelper::~NetworkStateTestHelper() {
  network_device_handler_.reset();
  if (!network_state_handler_)
    return;
  network_state_handler_->Shutdown();
  base::RunLoop().RunUntilIdle();  // Process any pending updates
  network_state_handler_.reset();
}

void NetworkStateTestHelper::AddDevice(const std::string& device_path,
                                       const std::string& type,
                                       const std::string& name) {
  device_test()->AddDevice(device_path, type, name);
  base::RunLoop().RunUntilIdle();
  network_state_handler_->SetDeviceStateUpdatedForTest(device_path);
}

}  // namespace ash
