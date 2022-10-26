// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_synchronizer_base.h"

#include <memory>

namespace ash::secure_channel {

BleSynchronizerBase::RegisterArgs::RegisterArgs(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    device::BluetoothAdapter::CreateAdvertisementCallback callback,
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback)
    : advertisement_data(std::move(advertisement_data)),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BleSynchronizerBase::RegisterArgs::~RegisterArgs() = default;

BleSynchronizerBase::UnregisterArgs::UnregisterArgs(
    scoped_refptr<device::BluetoothAdvertisement> advertisement,
    device::BluetoothAdvertisement::SuccessCallback callback,
    device::BluetoothAdvertisement::ErrorCallback error_callback)
    : advertisement(std::move(advertisement)),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BleSynchronizerBase::UnregisterArgs::~UnregisterArgs() = default;

BleSynchronizerBase::StartDiscoveryArgs::StartDiscoveryArgs(
    device::BluetoothAdapter::DiscoverySessionCallback callback,
    device::BluetoothAdapter::ErrorCallback error_callback)
    : callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BleSynchronizerBase::StartDiscoveryArgs::~StartDiscoveryArgs() = default;

BleSynchronizerBase::StopDiscoveryArgs::StopDiscoveryArgs(
    base::WeakPtr<device::BluetoothDiscoverySession> discovery_session,
    base::OnceClosure callback,
    device::BluetoothDiscoverySession::ErrorCallback error_callback)
    : discovery_session(discovery_session),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BleSynchronizerBase::StopDiscoveryArgs::~StopDiscoveryArgs() = default;

BleSynchronizerBase::Command::Command(
    std::unique_ptr<RegisterArgs> register_args)
    : command_type(CommandType::REGISTER_ADVERTISEMENT),
      register_args(std::move(register_args)) {}

BleSynchronizerBase::Command::Command(
    std::unique_ptr<UnregisterArgs> unregister_args)
    : command_type(CommandType::UNREGISTER_ADVERTISEMENT),
      unregister_args(std::move(unregister_args)) {}

BleSynchronizerBase::Command::Command(
    std::unique_ptr<StartDiscoveryArgs> start_discovery_args)
    : command_type(CommandType::START_DISCOVERY),
      start_discovery_args(std::move(start_discovery_args)) {}

BleSynchronizerBase::Command::Command(
    std::unique_ptr<StopDiscoveryArgs> stop_discovery_args)
    : command_type(CommandType::STOP_DISCOVERY),
      stop_discovery_args(std::move(stop_discovery_args)) {}

BleSynchronizerBase::Command::~Command() = default;

BleSynchronizerBase::BleSynchronizerBase() = default;

BleSynchronizerBase::~BleSynchronizerBase() = default;

void BleSynchronizerBase::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    device::BluetoothAdapter::CreateAdvertisementCallback callback,
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback) {
  command_queue_.push_back(
      std::make_unique<Command>(std::make_unique<RegisterArgs>(
          std::move(advertisement_data), std::move(callback),
          std::move(error_callback))));
  ProcessQueue();
}

void BleSynchronizerBase::UnregisterAdvertisement(
    scoped_refptr<device::BluetoothAdvertisement> advertisement,
    device::BluetoothAdvertisement::SuccessCallback callback,
    device::BluetoothAdvertisement::ErrorCallback error_callback) {
  command_queue_.push_back(
      std::make_unique<Command>(std::make_unique<UnregisterArgs>(
          std::move(advertisement), std::move(callback),
          std::move(error_callback))));
  ProcessQueue();
}

void BleSynchronizerBase::StartDiscoverySession(
    device::BluetoothAdapter::DiscoverySessionCallback callback,
    device::BluetoothAdapter::ErrorCallback error_callback) {
  command_queue_.emplace_back(
      std::make_unique<Command>(std::make_unique<StartDiscoveryArgs>(
          std::move(callback), std::move(error_callback))));
  ProcessQueue();
}

void BleSynchronizerBase::StopDiscoverySession(
    base::WeakPtr<device::BluetoothDiscoverySession> discovery_session,
    base::OnceClosure callback,
    device::BluetoothDiscoverySession::ErrorCallback error_callback) {
  command_queue_.emplace_back(
      std::make_unique<Command>(std::make_unique<StopDiscoveryArgs>(
          discovery_session, std::move(callback), std::move(error_callback))));
  ProcessQueue();
}

}  // namespace ash::secure_channel
