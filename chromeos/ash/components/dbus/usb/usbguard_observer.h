// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USB_USBGUARD_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USB_USBGUARD_OBSERVER_H_

#include <unistd.h>

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ash {

class COMPONENT_EXPORT(USB_CLIENT) UsbguardObserver
    : public base::CheckedObserver {
 public:
  // Based on usbguard src/Library/public/usbguard/Rule.hpp "enum class Target".
  // We should only see either kAllow or kBlock.
  enum class Target {
    kAllow = 0,
    kBlock = 1,
    kReject = 2,
    kMatch = 3,
    kUnknown = 4,
    kDevice = 5,
    kEmpty = 6,
    kInvalid = 7,
  };

  // A signal fired by usbguard-daemon whenever a USB device has been connected
  // after the resulting policy has been determined.
  //
  // Parameters
  //  |target_old| represents the state of the device before applying the policy
  //    rules. Typically, this is kBlock except for signals emitted as part of
  //    the usbguard-daemon service starting up.
  //  |target_new| represents the state of the device after applying policy
  //    rules.
  //  |device_rule| is a string representation of the usbguard rule that matches
  //    the device.
  //  |attributes| has key-value pairs with information about the device. Keys
  //    that can be expected include:
  //     "hash" - A hash of the USB device descriptors for example
  //       "9hMkYEMPjuNegGmzLIKwUp2MPctSL0tCWk7ruWGuOzc="
  //     "id" - The VID:PID for example "0781:5588"
  //     "name" - The text name of the device such as "USB Extreme Pro"
  //     "parent-hash" a hash the parent device's USB descriptors
  //     "serial" a serial number string that is often blank and doesn't have a
  //       reliable pattern.
  //     "via-port" the port number the device is connected to such as "2-1.1.3"
  //     "with-interface" a list of USB interfaces. Each interface includes the
  //       class, subclass, and protocol numbers used to bind an interface to a
  //       driver for example "08:06:50".
  virtual void DevicePolicyChanged(
      uint32_t id,
      Target target_old,
      Target target_new,
      const std::string& device_rule,
      uint32_t rule_id,
      const std::map<std::string, std::string>& attributes) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USB_USBGUARD_OBSERVER_H_
