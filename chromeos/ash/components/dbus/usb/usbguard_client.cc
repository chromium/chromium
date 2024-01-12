// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/usb/usbguard_client.h"

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/usb/fake_usbguard_client.h"
#include "chromeos/ash/components/dbus/usb/usbguard_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/usbguard/dbus-constants.h"

namespace ash {

namespace {
UsbguardClient* g_instance = nullptr;
}  // namespace

class UsbguardClientImpl : public UsbguardClient {
 public:
  UsbguardClientImpl() = default;

  UsbguardClientImpl(const UsbguardClientImpl&) = delete;
  UsbguardClientImpl& operator=(const UsbguardClientImpl&) = delete;

  ~UsbguardClientImpl() override = default;

  // UsbguardClient:
  void AddObserver(UsbguardObserver* observer) override {
    observers_.AddObserver(observer);
  }

  // UsbguardClient:
  void RemoveObserver(UsbguardObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  // UsbguardClient:
  bool HasObserver(const UsbguardObserver* observer) const override {
    return observers_.HasObserver(observer);
  }

  void Init(dbus::Bus* bus) {
    bus_ = bus;

    usbguard_proxy_ = bus_->GetObjectProxy(
        usbguard::kUsbguardServiceName,
        dbus::ObjectPath(usbguard::kUsbguardDevicesInterfacePath));

    usbguard_proxy_->ConnectToSignal(
        usbguard::kUsbguardDevicesInterface,
        usbguard::kDevicePolicyChangedSignalName,
        base::BindRepeating(&UsbguardClientImpl::DevicePolicyChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&UsbguardClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Dispatches the DevicePolicyChanged signal with signature: uuusua{ss}
  void DevicePolicyChanged(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);
    dbus::MessageReader array_reader(nullptr);

    uint32_t id;
    uint32_t target_old;
    uint32_t target_new;
    std::string device_rule;
    uint32_t rule_id;
    if (!signal_reader.PopUint32(&id) ||
        !signal_reader.PopUint32(&target_old) ||
        !signal_reader.PopUint32(&target_new) ||
        !signal_reader.PopString(&device_rule) ||
        !signal_reader.PopUint32(&rule_id) ||
        !signal_reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Error reading signal from usbguard: "
                 << signal->ToString();
      return;
    }

    std::map<std::string, std::string> attributes;
    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry(nullptr);
      std::string key;
      std::string value;
      if (!array_reader.PopDictEntry(&dict_entry) ||
          !dict_entry.PopString(&key) || !dict_entry.PopString(&value)) {
        LOG(ERROR) << "Error reading array from signal from usbguard: "
                   << signal->ToString();
        return;
      }
      attributes[key] = value;
    }

    for (auto& observer : observers_) {
      observer.DevicePolicyChanged(
          id, static_cast<UsbguardObserver::Target>(target_old),
          static_cast<UsbguardObserver::Target>(target_new), device_rule,
          rule_id, attributes);
    }
  }

  // Called when the biometrics signal is initially connected.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    LOG_IF(ERROR, !success)
        << "Failed to connect to usbguard signal: " << signal_name;
  }

  raw_ptr<dbus::Bus> bus_ = nullptr;
  raw_ptr<dbus::ObjectProxy> usbguard_proxy_ = nullptr;
  base::ObserverList<UsbguardObserver> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UsbguardClientImpl> weak_ptr_factory_{this};
};

UsbguardClient::UsbguardClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

UsbguardClient::~UsbguardClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void UsbguardClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new UsbguardClientImpl())->Init(bus);
}

// static
void UsbguardClient::InitializeFake() {
  new FakeUsbguardClient();
}

// static
void UsbguardClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
UsbguardClient* UsbguardClient::Get() {
  return g_instance;
}

}  // namespace ash
