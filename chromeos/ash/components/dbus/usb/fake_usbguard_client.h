// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USB_FAKE_USBGUARD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USB_FAKE_USBGUARD_CLIENT_H_

#include <map>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/usb/usbguard_client.h"
#include "chromeos/ash/components/dbus/usb/usbguard_observer.h"

namespace ash {

class COMPONENT_EXPORT(USB_CLIENT) FakeUsbguardClient : public UsbguardClient {
 public:
  FakeUsbguardClient();

  FakeUsbguardClient(const FakeUsbguardClient&) = delete;
  FakeUsbguardClient& operator=(const FakeUsbguardClient&) = delete;

  ~FakeUsbguardClient() override;

  // Returns the global instance if initialized. May return null.
  static FakeUsbguardClient* Get();

  // UsbguardClient:
  void AddObserver(UsbguardObserver* observer) override;
  void RemoveObserver(UsbguardObserver* observer) override;
  bool HasObserver(const UsbguardObserver* observer) const override;

  // Simulates receiving a DevicePolicyChanged signal with the given parameters.
  void SendDevicePolicyChanged(
      uint32_t id,
      UsbguardObserver::Target target_old,
      UsbguardObserver::Target target_new,
      const std::string& device_rule,
      uint32_t rule_id,
      const std::map<std::string, std::string>& attributes);

 private:
  base::ObserverList<UsbguardObserver> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USB_FAKE_USBGUARD_CLIENT_H_
