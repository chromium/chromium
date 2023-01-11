// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_SYNCHRONIZER_BASE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_SYNCHRONIZER_BASE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

namespace ash::secure_channel {

// Ensures that BLE advertisement registration/unregistration commands and
// discovery start/stop are not sent too close to each other. Because Bluetooth
// race conditions exist in the kernel, this strategy is necessary to work
// around potential bugs. Essentially, this class is a synchronization wrapper
// around the Bluetooth API.
class BleSynchronizerBase {
 public:
  BleSynchronizerBase();

  BleSynchronizerBase(const BleSynchronizerBase&) = delete;
  BleSynchronizerBase& operator=(const BleSynchronizerBase&) = delete;

  virtual ~BleSynchronizerBase();

  // Advertisement wrappers.
  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      device::BluetoothAdapter::CreateAdvertisementCallback callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback);
  void UnregisterAdvertisement(
      scoped_refptr<device::BluetoothAdvertisement> advertisement,
      device::BluetoothAdvertisement::SuccessCallback success_callback,
      device::BluetoothAdvertisement::ErrorCallback error_callback);

  // Discovery session wrappers.
  void StartDiscoverySession(
      device::BluetoothAdapter::DiscoverySessionCallback callback,
      device::BluetoothAdapter::ErrorCallback error_callback);
  void StopDiscoverySession(
      base::WeakPtr<device::BluetoothDiscoverySession> discovery_session,
      base::OnceClosure callback,
      device::BluetoothDiscoverySession::ErrorCallback error_callback);

 protected:
  enum class CommandType {
    REGISTER_ADVERTISEMENT,
    UNREGISTER_ADVERTISEMENT,
    START_DISCOVERY,
    STOP_DISCOVERY
  };

  struct RegisterArgs {
    RegisterArgs(
        std::unique_ptr<device::BluetoothAdvertisement::Data>
            advertisement_data,
        device::BluetoothAdapter::CreateAdvertisementCallback callback,
        device::BluetoothAdapter::AdvertisementErrorCallback error_callback);
    virtual ~RegisterArgs();

    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data;
    device::BluetoothAdapter::CreateAdvertisementCallback callback;
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
  };

  struct UnregisterArgs {
    UnregisterArgs(
        scoped_refptr<device::BluetoothAdvertisement> advertisement,
        device::BluetoothAdvertisement::SuccessCallback callback,
        device::BluetoothAdvertisement::ErrorCallback error_callback);
    virtual ~UnregisterArgs();

    scoped_refptr<device::BluetoothAdvertisement> advertisement;
    device::BluetoothAdvertisement::SuccessCallback callback;
    device::BluetoothAdvertisement::ErrorCallback error_callback;
  };

  struct StartDiscoveryArgs {
    StartDiscoveryArgs(
        device::BluetoothAdapter::DiscoverySessionCallback callback,
        device::BluetoothAdapter::ErrorCallback error_callback);
    virtual ~StartDiscoveryArgs();

    device::BluetoothAdapter::DiscoverySessionCallback callback;
    device::BluetoothAdapter::ErrorCallback error_callback;
  };

  struct StopDiscoveryArgs {
    StopDiscoveryArgs(
        base::WeakPtr<device::BluetoothDiscoverySession> discovery_session,
        base::OnceClosure callback,
        device::BluetoothDiscoverySession::ErrorCallback error_callback);
    virtual ~StopDiscoveryArgs();

    base::WeakPtr<device::BluetoothDiscoverySession> discovery_session;
    base::OnceClosure callback;
    device::BluetoothDiscoverySession::ErrorCallback error_callback;
  };

  struct Command {
    explicit Command(std::unique_ptr<RegisterArgs> register_args);
    explicit Command(std::unique_ptr<UnregisterArgs> unregister_args);
    explicit Command(std::unique_ptr<StartDiscoveryArgs> start_discovery_args);
    explicit Command(std::unique_ptr<StopDiscoveryArgs> stop_discovery_args);
    virtual ~Command();

    CommandType command_type;
    std::unique_ptr<RegisterArgs> register_args;
    std::unique_ptr<UnregisterArgs> unregister_args;
    std::unique_ptr<StartDiscoveryArgs> start_discovery_args;
    std::unique_ptr<StopDiscoveryArgs> stop_discovery_args;
  };

  // Processes the next item in the queue.
  virtual void ProcessQueue() = 0;

  std::deque<std::unique_ptr<Command>>& command_queue() {
    return command_queue_;
  }

 private:
  std::deque<std::unique_ptr<Command>> command_queue_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_SYNCHRONIZER_BASE_H_
