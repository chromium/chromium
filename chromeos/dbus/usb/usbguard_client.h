// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USB_USBGUARD_CLIENT_H_
#define CHROMEOS_DBUS_USB_USBGUARD_CLIENT_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/usb/usbguard_observer.h"

namespace dbus {
class Bus;
}

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) UsbguardClient {
 public:
  virtual ~UsbguardClient();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static UsbguardClient* Get();

  // Adds the given observer.
  virtual void AddObserver(UsbguardObserver* observer) = 0;
  // Removes the given observer if this object has the observer.
  virtual void RemoveObserver(UsbguardObserver* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const UsbguardObserver* observer) const = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  UsbguardClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbguardClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USB_USBGUARD_CLIENT_H_
