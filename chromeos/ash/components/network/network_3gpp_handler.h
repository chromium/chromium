// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_3GPP_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_3GPP_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/modem_3gpp_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace ash {

// This class handles call to ModemManager's SetCarrierLock method which
// configures network personalisation lock on cellular modem.
// This method is part of Modem 3gpp interface.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) Network3gppHandler
    : public ShillPropertyChangedObserver {
 public:
  Network3gppHandler();
  Network3gppHandler(const Network3gppHandler&) = delete;
  Network3gppHandler& operator=(const Network3gppHandler&) = delete;

  ~Network3gppHandler() override;

  virtual void SetCarrierLock(const std::string& configuration,
                              Modem3gppClient::CarrierLockCallback callback);

 private:
  friend class NetworkHandler;
  friend class Network3gppHandlerTest;

  class Network3gppDeviceHandler;
  class Network3gppDeviceHandlerImpl;

  // ShillPropertyChangedObserver:
  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override;

  // Requests the devices from the network manager, sets up observers, and
  // requests the initial list of messages.
  void Init();

  // Callback to handle the manager properties with the list of devices.
  void ManagerPropertiesCallback(std::optional<base::Value::Dict> properties);

  // Requests properties for each entry in |devices|.
  void UpdateDevices(const base::Value::List& devices);

  // Callback to handle the device properties for |device_path|.
  // A Network3gppDeviceHandler will be instantiated for each cellular device.
  void DevicePropertiesCallback(const std::string& device_path,
                                std::optional<base::Value::Dict> properties);

  // Called when the cellular device's object path changes. This means that
  // there has been an update to the device's SIM (removed or inserted) and that
  // a new handler should be created for the device's new object path.
  void OnObjectPathChanged(const base::Value& object_path);

  raw_ptr<Modem3gppClient> modem_client_;
  std::unique_ptr<Network3gppDeviceHandler> device_handler_;
  std::string cellular_device_path_;
  base::WeakPtrFactory<Network3gppHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_3GPP_HANDLER_H_
