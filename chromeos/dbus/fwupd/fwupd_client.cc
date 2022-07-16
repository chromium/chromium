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

  void RequestUpgrades(std::string device_id) override {
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetUpgradesMethodName);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_id);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestUpgradesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void RequestDevices() override {
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetDevicesMethodName);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestDevicesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Pops a string-to-variant-string dictionary from the reader.
  std::unique_ptr<base::DictionaryValue> PopStringToStringDictionary(
      dbus::MessageReader* reader) {
    dbus::MessageReader array_reader(nullptr);

    if (!reader->PopArray(&array_reader)) {
      LOG(ERROR) << "Failed to pop array into the array reader.";
      return nullptr;
    }

    auto result = std::make_unique<base::DictionaryValue>();

    while (array_reader.HasMoreData()) {
      dbus::MessageReader entry_reader(nullptr);
      dbus::MessageReader variant_reader(nullptr);
      std::string key;
      std::string value;

      const bool success = array_reader.PopDictEntry(&entry_reader) &&
                           entry_reader.PopString(&key) &&
                           entry_reader.PopVariant(&variant_reader) &&
                           variant_reader.PopString(&value);

      if (success)
        result->SetKey(key, base::Value(value));
      else
        LOG(ERROR) << "Failed to get a dictionary entry.";
    }
    return result;
  }

  void RequestUpgradesCallback(dbus::Response* response,
                               dbus::ErrorResponse* error_response) {
    if (!response) {
      LOG(ERROR) << "No Dbus response received from fwupd.";
      return;
    }

    // TODO(swifton): This is a stub implementation. Replace this with a
    // callback call for FirmwareUpdateHandler when it's implemented.
    ++request_upgrades_callback_call_count_for_testing_;
  }

  void RequestDevicesCallback(dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (!response) {
      LOG(ERROR) << "No Dbus response received from fwupd.";
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);

    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Failed to parse string from DBus Signal";
      return;
    }

    auto devices = std::make_unique<FwupdDeviceList>();

    while (array_reader.HasMoreData()) {
      // Parse device description.
      std::unique_ptr<base::DictionaryValue> dict(
          PopStringToStringDictionary(&array_reader));
      if (!dict) {
        LOG(ERROR) << "Failed to parse the device description.";
        return;
      }

      const auto* id = dict->FindKey("DeviceId");
      const auto* name = dict->FindKey("Name");

      if (id && name) {
        devices->push_back(FwupdDevice(id->GetString(), name->GetString()));
      } else {
        LOG(ERROR) << "No device id or name found.";
        return;
      }
    }

    for (auto& observer : observers_)
      observer.OnDeviceListResponse(devices.get());
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

void FwupdClient::AddObserver(FwupdClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FwupdClient::RemoveObserver(FwupdClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

FwupdClient::FwupdClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FwupdClient::~FwupdClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

FwupdClientImpl::FwupdClientImpl() = default;

FwupdClientImpl::~FwupdClientImpl() = default;

// static
std::unique_ptr<FwupdClient> FwupdClient::Create() {
  return std::make_unique<FwupdClientImpl>();
}

// static
FwupdClient* FwupdClient::Get() {
  return g_instance;
}

}  // namespace chromeos
