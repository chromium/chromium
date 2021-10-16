// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_client.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

FwupdClient* g_instance = nullptr;

const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
const char kFwupdServiceInterface[] = "org.freedesktop.fwupd";
const char kFwupdDeviceAddedSignalName[] = "DeviceAdded";
const char kFwupdGetUpgradesMethodName[] = "GetUpgrades";
const char kFwupdGetDevicesMethodName[] = "GetDevices";

}  // namespace

class FwupdClientImpl : public FwupdClient {
 public:
  FwupdClientImpl();
  FwupdClientImpl(const FwupdClientImpl&) = delete;
  FwupdClientImpl& operator=(const FwupdClientImpl&) = delete;
  ~FwupdClientImpl() override;

 protected:
  void Init(dbus::Bus* bus) override {
    if (!features::IsFirmwareUpdaterAppEnabled())
      return;

    proxy_ = bus->GetObjectProxy(kFwupdServiceName,
                                 dbus::ObjectPath(kFwupdServicePath));

    proxy_->ConnectToSignal(
        kFwupdServiceInterface, kFwupdDeviceAddedSignalName,
        base::BindRepeating(&FwupdClientImpl::OnDeviceAddedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FwupdClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetUpgrades(std::string device_id) override {
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetUpgradesMethodName);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_id);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::GetUpgradesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetDevices() override {
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetDevicesMethodName);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::GetDevicesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void GetUpgradesCallback(dbus::Response* response,
                           dbus::ErrorResponse* error_response) {
    if (!response) {
      LOG(ERROR) << "No Dbus response received from fwupd.";
      return;
    }

    // TODO(swifton): This is a stub implementation. Replace this with a
    // callback call for FirmwareUpdateHandler when it's implemented.
    ++get_upgrades_callback_call_count_for_testing_;
  }

  void GetDevicesCallback(dbus::Response* response,
                          dbus::ErrorResponse* error_response) {
    if (!response) {
      LOG(ERROR) << "No Dbus response received from fwupd.";
      return;
    }

    // TODO(swifton): This is a stub implementation. Replace this with a
    // callback call for FirmwareUpdateHandler when it's implemented.
    ++get_devices_callback_call_count_for_testing_;
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    if (!is_connected) {
      LOG(ERROR) << "Failed to connect to signal " << signal_name;
    }
    DCHECK_EQ(kFwupdServiceInterface, interface_name);
  }

  // TODO(swifton): This is a stub implementation.
  void OnDeviceAddedReceived(dbus::Signal* signal) {
    if (client_is_in_testing_mode_) {
      ++device_signal_call_count_for_testing_;
    }
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FwupdClientImpl> weak_ptr_factory_{this};
};

FwupdClientImpl::FwupdClientImpl() {
  DCHECK(!g_instance);
  g_instance = this;
}

FwupdClientImpl::~FwupdClientImpl() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
std::unique_ptr<FwupdClient> FwupdClient::Create() {
  return std::make_unique<FwupdClientImpl>();
}

// static
FwupdClient* FwupdClient::Get() {
  return g_instance;
}

}  // namespace chromeos
