// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_3gpp_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/modem_3gpp_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class Network3gppHandler::Network3gppDeviceHandler {
 public:
  Network3gppDeviceHandler() = default;
  virtual ~Network3gppDeviceHandler() = default;

  virtual void SetCarrierLock(
      const std::string& config,
      Modem3gppClient::CarrierLockCallback callback) = 0;
};

class Network3gppHandler::Network3gppDeviceHandlerImpl
    : public Network3gppHandler::Network3gppDeviceHandler {
 public:
  Network3gppDeviceHandlerImpl(const std::string& service_name,
                               const dbus::ObjectPath& object_path,
                               Modem3gppClient* modem_client);

  Network3gppDeviceHandlerImpl(const Network3gppDeviceHandlerImpl&) = delete;
  Network3gppDeviceHandlerImpl& operator=(const Network3gppDeviceHandlerImpl&) =
      delete;

 private:
  // Network3gppHandler::Network3gppDeviceHandler:
  void SetCarrierLock(const std::string& config,
                      Modem3gppClient::CarrierLockCallback callback) override;

  std::string service_name_;
  dbus::ObjectPath object_path_;
  raw_ptr<Modem3gppClient> modem_client_;

  base::WeakPtrFactory<Network3gppDeviceHandlerImpl> weak_ptr_factory_{this};
};

Network3gppHandler::Network3gppDeviceHandlerImpl::Network3gppDeviceHandlerImpl(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    Modem3gppClient* modem_client)
    : service_name_(service_name),
      object_path_(object_path),
      modem_client_(modem_client) {}

void Network3gppHandler::Network3gppDeviceHandlerImpl::SetCarrierLock(
    const std::string& config,
    Modem3gppClient::CarrierLockCallback callback) {
  if (!modem_client_) {
    NET_LOG(ERROR) << "Modem 3gpp client not initialized.";
    std::move(callback).Run(CarrierLockResult::kNotInitialized);
    return;
  }

  modem_client_->SetCarrierLock(service_name_, object_path_, config,
                                std::move(callback));
}

///////////////////////////////////////////////////////////////////////////////
// Network3gppHandler

Network3gppHandler::Network3gppHandler() {}

Network3gppHandler::~Network3gppHandler() {
  if (!ShillManagerClient::Get()) {
    return;
  }

  ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  if (!cellular_device_path_.empty()) {
    ShillDeviceClient::Get()->RemovePropertyChangedObserver(
        dbus::ObjectPath(cellular_device_path_), this);
  }
}

void Network3gppHandler::Init() {
  // Add as an observer here so that new devices added after this call are
  // recognized.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);

  // Request network manager properties so that we can get the list of devices.
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&Network3gppHandler::ManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Network3gppHandler::SetCarrierLock(
    const std::string& configuration,
    Modem3gppClient::CarrierLockCallback callback) {
  if (!device_handler_) {
    NET_LOG(ERROR) << "ModemManager device handler not initialized.";
    std::move(callback).Run(CarrierLockResult::kNotInitialized);
    return;
  }

  device_handler_->SetCarrierLock(configuration, std::move(callback));
}

void Network3gppHandler::OnPropertyChanged(const std::string& name,
                                           const base::Value& value) {
  // Device property change
  if (name == shill::kDBusObjectProperty) {
    OnObjectPathChanged(value);
    return;
  }

  // Manager property change
  if (name == shill::kDevicesProperty && value.is_list()) {
    UpdateDevices(value.GetList());
  }
}

void Network3gppHandler::ManagerPropertiesCallback(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Network3gppHandler: Failed to get manager properties.";
    return;
  }
  const base::Value::List* value =
      properties->FindList(shill::kDevicesProperty);
  if (!value) {
    NET_LOG(EVENT) << "Network3gppHandler: No list value for: "
                   << shill::kDevicesProperty;
    return;
  }
  UpdateDevices(*value);
}

void Network3gppHandler::UpdateDevices(const base::Value::List& devices) {
  for (const auto& item : devices) {
    if (!item.is_string()) {
      continue;
    }

    const std::string device_path = item.GetString();
    if (device_path.empty()) {
      continue;
    }
    // Request device properties.
    NET_LOG(DEBUG) << "GetDeviceProperties: " << device_path;
    ShillDeviceClient::Get()->GetProperties(
        dbus::ObjectPath(device_path),
        base::BindOnce(&Network3gppHandler::DevicePropertiesCallback,
                       weak_ptr_factory_.GetWeakPtr(), device_path));
  }
}

void Network3gppHandler::DevicePropertiesCallback(
    const std::string& device_path,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Network3gppHandler error for: " << device_path;
    return;
  }

  const std::string* device_type = properties->FindString(shill::kTypeProperty);
  if (!device_type) {
    NET_LOG(ERROR) << "Network3gppHandler: No type for: " << device_path;
    return;
  }
  if (*device_type != shill::kTypeCellular) {
    return;
  }

  const std::string* service_name =
      properties->FindString(shill::kDBusServiceProperty);
  if (!service_name) {
    NET_LOG(ERROR) << "Device has no DBusService Property: " << device_path;
    return;
  }

  if (*service_name != modemmanager::kModemManager1ServiceName) {
    return;
  }

  const std::string* object_path_string =
      properties->FindString(shill::kDBusObjectProperty);
  if (!object_path_string || object_path_string->empty()) {
    NET_LOG(ERROR) << "Device has no or empty DBusObject Property: "
                   << device_path;
    return;
  }
  dbus::ObjectPath object_path(*object_path_string);

  modem_client_ = Modem3gppClient::Get();
  if (!modem_client_) {
    NET_LOG(ERROR) << "Modem shill client was not initialized!";
    return;
  }

  device_handler_ = std::make_unique<Network3gppDeviceHandlerImpl>(
      *service_name, object_path, modem_client_);

  if (!cellular_device_path_.empty()) {
    ShillDeviceClient::Get()->RemovePropertyChangedObserver(
        dbus::ObjectPath(cellular_device_path_), this);
  }
  cellular_device_path_ = device_path;
  ShillDeviceClient::Get()->AddPropertyChangedObserver(
      dbus::ObjectPath(cellular_device_path_), this);
}

void Network3gppHandler::OnObjectPathChanged(const base::Value& object_path) {
  // Remove the old handler.
  device_handler_.reset();

  const std::string object_path_string =
      object_path.is_string() ? object_path.GetString() : std::string();
  // If the new object path is empty, there is no SIM. Don't create a new
  // handler.
  if (object_path_string.empty() || object_path_string == "/") {
    return;
  }

  // Recreate handler for the new object path.
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&Network3gppHandler::ManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
