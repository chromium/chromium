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
#include "chromeos/ash/components/network/technology_state_controller.h"

namespace ash {

FakeHotspotOperationDelegate::FakeHotspotOperationDelegate(
    TechnologyStateController* technology_state_controller) {
  technology_state_controller_ = technology_state_controller;
  technology_state_controller_->set_hotspot_operation_delegate(this);
}

FakeHotspotOperationDelegate::~FakeHotspotOperationDelegate() {
  technology_state_controller_->set_hotspot_operation_delegate(nullptr);
}

void FakeHotspotOperationDelegate::PrepareEnableWifi(
    base::OnceCallback<void(bool prepare_success)> callback) {
  std::move(callback).Run(/*success=*/true);
}

NetworkStateTestHelper::NetworkStateTestHelper(
    bool use_default_devices_and_services) {
  AddDefaultProfiles();

  network_state_handler_ = NetworkStateHandler::InitializeForTest();
  network_device_handler_ =
      NetworkDeviceHandler::InitializeForTesting(network_state_handler_.get());
  technology_state_controller_ = std::make_unique<TechnologyStateController>();
  technology_state_controller_->Init(network_state_handler_.get());

  fake_hotspot_operation_delegate_ =
      std::make_unique<FakeHotspotOperationDelegate>(
          technology_state_controller_.get());

  if (!use_default_devices_and_services)
    ResetDevicesAndServices();
}

NetworkStateTestHelper::~NetworkStateTestHelper() {
  fake_hotspot_operation_delegate_.reset();
  network_device_handler_.reset();
  technology_state_controller_.reset();
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
