// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SMS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SMS_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/values.h"

namespace base {
class Bus;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}

namespace ash {

// SMSMessageClient is used to communicate with the
// org.freedesktop.ModemManager1.SMS service.  All methods should be
// called from the origin thread (UI thread) which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) SMSClient {
 public:
  using GetAllCallback = base::OnceCallback<void(const base::Value::Dict& sms)>;

  static const char kSMSPropertyState[];
  static const char kSMSPropertyNumber[];
  static const char kSMSPropertyText[];
  static const char kSMSPropertyTimestamp[];

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static SMSClient* Get();

  SMSClient(const SMSClient&) = delete;
  SMSClient& operator=(const SMSClient&) = delete;

  // Calls GetAll method.  |callback| is called after the method call succeeds.
  virtual void GetAll(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      GetAllCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  SMSClient();
  virtual ~SMSClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SMS_CLIENT_H_
