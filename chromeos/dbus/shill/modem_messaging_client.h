// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_MODEM_MESSAGING_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_MODEM_MESSAGING_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// ModemMessagingClient is used to communicate with the
// org.freedesktop.ModemManager1.Modem.Messaging service.  All methods
// should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ModemMessagingClient {
 public:
  typedef base::Callback<void(const dbus::ObjectPath& message_path,
                              bool complete)>
      SmsReceivedHandler;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ModemMessagingClient* Get();

  // Sets SmsReceived signal handler.
  virtual void SetSmsReceivedHandler(const std::string& service_name,
                                     const dbus::ObjectPath& object_path,
                                     const SmsReceivedHandler& handler) = 0;

  // Resets SmsReceived signal handler.
  virtual void ResetSmsReceivedHandler(const std::string& service_name,
                                       const dbus::ObjectPath& object_path) = 0;

  // Calls Delete method.  |callback| is called on method completion.
  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      const dbus::ObjectPath& sms_path,
                      VoidDBusMethodCallback callback) = 0;

  // Calls List method.  |callback| is called on method completion.
  using ListCallback = DBusMethodCallback<std::vector<dbus::ObjectPath>>;
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    ListCallback callback) = 0;

 protected:
  friend class ModemMessagingClientTest;

  // Initialize/Shutdown should be used instead.
  ModemMessagingClient();
  virtual ~ModemMessagingClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ModemMessagingClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_MODEM_MESSAGING_CLIENT_H_
