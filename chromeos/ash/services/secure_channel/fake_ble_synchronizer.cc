// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_ble_synchronizer.h"

namespace ash::secure_channel {

FakeBleSynchronizer::FakeBleSynchronizer() = default;

FakeBleSynchronizer::~FakeBleSynchronizer() = default;

size_t FakeBleSynchronizer::GetNumCommands() {
  return command_queue().size();
}

device::BluetoothAdvertisement::Data& FakeBleSynchronizer::GetAdvertisementData(
    size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type ==
         CommandType::REGISTER_ADVERTISEMENT);
  return *command_queue()[index]->register_args->advertisement_data;
}

device::BluetoothAdapter::CreateAdvertisementCallback
FakeBleSynchronizer::GetRegisterCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type ==
         CommandType::REGISTER_ADVERTISEMENT);
  return std::move(command_queue()[index]->register_args->callback);
}

device::BluetoothAdapter::AdvertisementErrorCallback
FakeBleSynchronizer::GetRegisterErrorCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type ==
         CommandType::REGISTER_ADVERTISEMENT);
  return std::move(command_queue()[index]->register_args->error_callback);
}

device::BluetoothAdvertisement::SuccessCallback
FakeBleSynchronizer::GetUnregisterCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type ==
         CommandType::UNREGISTER_ADVERTISEMENT);
  return std::move(command_queue()[index]->unregister_args->callback);
}

device::BluetoothAdvertisement::ErrorCallback
FakeBleSynchronizer::GetUnregisterErrorCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type ==
         CommandType::UNREGISTER_ADVERTISEMENT);
  return std::move(command_queue()[index]->unregister_args->error_callback);
}

device::BluetoothAdapter::DiscoverySessionCallback
FakeBleSynchronizer::TakeStartDiscoveryCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type == CommandType::START_DISCOVERY);
  return std::move(command_queue()[index]->start_discovery_args->callback);
}

device::BluetoothAdapter::ErrorCallback
FakeBleSynchronizer::TakeStartDiscoveryErrorCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type == CommandType::START_DISCOVERY);
  return std::move(
      command_queue()[index]->start_discovery_args->error_callback);
}

base::OnceClosure FakeBleSynchronizer::GetStopDiscoveryCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type == CommandType::STOP_DISCOVERY);
  return std::move(command_queue()[index]->stop_discovery_args->callback);
}

device::BluetoothDiscoverySession::ErrorCallback
FakeBleSynchronizer::GetStopDiscoveryErrorCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index]->command_type == CommandType::STOP_DISCOVERY);
  return std::move(command_queue()[index]->stop_discovery_args->error_callback);
}

// Left intentionally blank. The test double does not need to process any queued
// commands.
void FakeBleSynchronizer::ProcessQueue() {}

}  // namespace ash::secure_channel
