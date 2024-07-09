// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_MODEM_MESSAGING_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_MODEM_MESSAGING_CLIENT_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

// ModemMessagingClient is used to communicate with the
// org.freedesktop.ModemManager1.Modem.Messaging service.  All methods
// should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ModemMessagingClient {
 public:
  using SmsReceivedHandler =
      base::RepeatingCallback<void(const dbus::ObjectPath& message_path,
                                   bool complete)>;

  // Interface for performing modem messaging actions for testing.
  // Accessed through GetTestInterface(), only implemented in the Stub Impl.
  class TestInterface {
   public:
    virtual void ReceiveSms(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& sms_path) = 0;

    // Returns the SMS path of the SMS currently pending deletion.
    virtual std::string GetPendingDeleteRequestSmsPath() const = 0;

    // Completes the pending deletion.
    virtual void CompletePendingDeleteRequest(bool success) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ModemMessagingClient* Get();

  ModemMessagingClient(const ModemMessagingClient&) = delete;
  ModemMessagingClient& operator=(const ModemMessagingClient&) = delete;

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
                      chromeos::VoidDBusMethodCallback callback) = 0;

  // Calls List method.  |callback| is called on method completion.
  using ListCallback =
      chromeos::DBusMethodCallback<std::vector<dbus::ObjectPath>>;
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    ListCallback callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ModemMessagingClientTest;

  // Initialize/Shutdown should be used instead.
  ModemMessagingClient();
  virtual ~ModemMessagingClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_MODEM_MESSAGING_CLIENT_H_
