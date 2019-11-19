// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USB_FAKE_USBGUARD_CLIENT_H_
#define CHROMEOS_DBUS_USB_FAKE_USBGUARD_CLIENT_H_

#include <map>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/usb/usbguard_client.h"
#include "chromeos/dbus/usb/usbguard_observer.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeUsbguardClient
    : public UsbguardClient {
 public:
  FakeUsbguardClient();
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

  DISALLOW_COPY_AND_ASSIGN(FakeUsbguardClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USB_FAKE_USBGUARD_CLIENT_H_
