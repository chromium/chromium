// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_COMMON_DBUS_CLIENT_H_

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// Interface for all DBus clients handled by DBusThreadManager. It restricts
// access to the Init function to DBusThreadManager only to prevent
// incorrect calls. Stub clients may lift that restriction however.
class DBusClient {
 public:
  DBusClient& operator=(const DBusClient&) = delete;

 protected:
  virtual ~DBusClient() {}

  // This function is called by DBusThreadManager. Only in unit tests, which
  // don't use DBusThreadManager, this function can be called through Stub
  // implementations (they change Init's member visibility to public).
  virtual void Init(dbus::Bus* bus) = 0;

 private:
  friend class DBusClientsBrowser;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_COMMON_DBUS_CLIENT_H_
