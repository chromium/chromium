// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SMS_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SMS_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace base {
class Bus;
class DictionaryValue;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}

namespace chromeos {

// SMSMessageClient is used to communicate with the
// org.freedesktop.ModemManager1.SMS service.  All methods should be
// called from the origin thread (UI thread) which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) SMSClient {
 public:
  using GetAllCallback =
      base::OnceCallback<void(const base::DictionaryValue& sms)>;

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

  // Calls GetAll method.  |callback| is called after the method call succeeds.
  virtual void GetAll(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      GetAllCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  SMSClient();
  virtual ~SMSClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SMSClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SMS_CLIENT_H_
