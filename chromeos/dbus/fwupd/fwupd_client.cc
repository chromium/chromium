// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_client.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/fwupd/dbus_constants.h"
#include "chromeos/dbus/fwupd/fwupd_properties.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

FwupdClient* g_instance = nullptr;

}  // namespace

class FwupdClientImpl : public FwupdClient {
 public:
  FwupdClientImpl();
  FwupdClientImpl(const FwupdClientImpl&) = delete;
  FwupdClientImpl& operator=(const FwupdClientImpl&) = delete;
  ~FwupdClientImpl() override;

 protected:
  void Init(dbus::Bus* bus) override {
    DCHECK(bus);

    proxy_ = bus->GetObjectProxy(kFwupdServiceName,
                                 dbus::ObjectPath(kFwupdServicePath));
    DCHECK(proxy_);
    proxy_->ConnectToSignal(
        kFwupdServiceInterface, kFwupdDeviceAddedSignalName,
        base::BindRepeating(&FwupdClientImpl::OnDeviceAddedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FwupdClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    properties_ = std::make_unique<FwupdProperties>(
        proxy_, base::BindRepeating(&FwupdClientImpl::OnPropertyChanged,
                                    weak_ptr_factory_.GetWeakPtr()));
    properties_->ConnectSignals();
    properties_->GetAll();
  }

  void RequestUpdates(const std::string& device_id) override {
    CHECK(features::IsFirmwareUpdaterAppEnabled());
    VLOG(1) << "fwupd: RequestUpdates called for: " << device_id;
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetUpgradesMethodName);
    dbus::MessageWriter writer(&method_call);

    writer.AppendString(device_id);

    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestUpdatesCallback,
                       weak_ptr_factory_.GetWeakPtr(), device_id));
  }

  void RequestDevices() override {
    CHECK(features::IsFirmwareUpdaterAppEnabled());
    VLOG(1) << "fwupd: RequestDevices called";
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdGetDevicesMethodName);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FwupdClientImpl::RequestDevicesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void InstallUpdate(const std::string& device_id,
                     base::ScopedFD file_descriptor,
                     FirmwareInstallOptions options) override {
    VLOG(1) << "fwupd: InstallUpdate called for id: " << device_id;
    dbus::MethodCall method_call(kFwupdServiceInterface,
                                 kFwupdInstallMethodName);
    dbus::MessageWriter writer(&method_call);

    writer.AppendString(device_id);
    writer.AppendFileDescriptor(file_descriptor.get());

    // Write the options in form of "a{sv}".
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("{sv}", &array_writer);
    for (const auto& option : options) {
      dbus::MessageWriter dict_entry_writer(nullptr);
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(option.first);
      dict_entry_writer.AppendVariantOfBool(option.second);
      array_writer.CloseContainer(&dict_entry_writer);
    }
    writer.CloseContainer(&array_writer);

  // TODO(michaelcheco): Investigate whether or not the estimated install time
  // multiplied by some factor can be used in place of |TIMEOUT_INFINITE|.
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&FwupdClientImpl::InstallUpdateCallback,
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
      std::string value_string;
      uint32_t value_uint = 0;

      const bool success = array_reader.PopDictEntry(&entry_reader) &&
                           entry_reader.PopString(&key) &&
                           entry_reader.PopVariant(&variant_reader);

      if (!success) {
        LOG(ERROR) << "Failed to get a dictionary entry. ";
        return nullptr;
      }

      // Values in the response can have different types. The fields we are
      // interested in, are all either strings (s) or uint32 (u). Some fields in
      // the response have other types, but we don't use them, so we just skip
      // them.
      if (variant_reader.GetDataSignature() == "u") {
        variant_reader.PopUint32(&value_uint);
        // Value doesn't support unsigned numbers, so this has to be converted
        // to int.
        result->SetKey(key, base::Value((int)value_uint));
      } else if (variant_reader.GetDataSignature() == "s") {
        variant_reader.PopString(&value_string);
        result->SetKey(key, base::Value(value_string));
      }
    }
    return result;
  }

  void RequestUpdatesCallback(const std::string& device_id,
                              dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    bool can_parse = true;
    if (!response) {
      LOG(ERROR) << "No Dbus response received from fwupd.";
      can_parse = false;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);

    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Failed to parse string from DBus Signal";
      can_parse = false;
    }

    FwupdUpdateList updates;
    while (can_parse && array_reader.HasMoreData()) {
      // Parse update description.
      std::unique_ptr<base::DictionaryValue> dict =
          PopStringToStringDictionary(&array_reader);
      if (!dict) {
        LOG(ERROR) << "Failed to parse the update description.";
        // Ran into an error, exit early.
        break;
      }

      const auto* version = dict->FindKey("Version");
      const auto* description = dict->FindKey("Description");
      const auto* priority = dict->FindKey("Urgency");

      const bool success = version && description && priority;
      // TODO(michaelcheco): Confirm that this is the expected behavior.
      if (success) {
        VLOG(1) << "fwupd: Found update version for device: " << device_id
                << " with version: " << version->GetString();
        updates.emplace_back(version->GetString(), description->GetString(),
                             priority->GetInt());
      } else {
        LOG(ERROR) << "Update version, description or priority is not found.";
      }
    }

    for (auto& observer : observers_) {
      observer.OnUpdateListResponse(device_id, &updates);
    }
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

    FwupdDeviceList devices;
    while (array_reader.HasMoreData()) {
      // Parse device description.
      std::unique_ptr<base::DictionaryValue> dict =
          PopStringToStringDictionary(&array_reader);
      if (!dict) {
        LOG(ERROR) << "Failed to parse the device description.";
        return;
      }

      const auto* id = dict->FindKey("DeviceId");
      const auto* name = dict->FindKey("Name");

      // The keys "DeviceId" and "Name" must exist in the dictionary.
      const bool success = id && name;
      if (!success) {
        LOG(ERROR) << "No device id or name found.";
        return;
      }

      VLOG(1) << "fwupd: Device found: " << id->GetString() << " "
              << name->GetString();
      devices.emplace_back(id->GetString(), name->GetString());
    }

    for (auto& observer : observers_)
      observer.OnDeviceListResponse(&devices);
  }

  void InstallUpdateCallback(dbus::Response* response,
                             dbus::ErrorResponse* error_response) {
    bool success = true;
    if (error_response) {
      LOG(ERROR) << "Firmware install failed with error: "
                 << error_response->GetErrorName();
      success = false;
    }

    VLOG(1) << "fwupd: InstallUpdate returned with: " << success;
    for (auto& observer : observers_)
      observer.OnInstallResponse(success);
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    if (!is_connected) {
      LOG(ERROR) << "Failed to connect to signal " << signal_name;
    }
  }

  // TODO(swifton): This is a stub implementation.
  void OnDeviceAddedReceived(dbus::Signal* signal) {
    // Do nothing if the feature is not enabled.
    if (!features::IsFirmwareUpdaterAppEnabled())
      return;

    if (client_is_in_testing_mode_) {
      ++device_signal_call_count_for_testing_;
    }
  }

  void OnPropertyChanged(const std::string& name) {
    if (!features::IsFirmwareUpdaterAppEnabled())
      return;

    for (auto& observer : observers_)
      observer.OnPropertiesChangedResponse(properties_.get());
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
