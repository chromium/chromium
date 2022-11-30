// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_INIT_INITIALIZE_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_INIT_INITIALIZE_DBUS_CLIENT_H_

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// Initializes the appropriate version of D-Bus client.
template <typename T>
void InitializeDBusClient(dbus::Bus* bus) {
#if defined(USE_REAL_DBUS_CLIENTS)
  T::Initialize(bus);
#else
  // TODO(hashimoto): Always use fakes after adding
  // use_real_dbus_clients=true to where needed. crbug.com/952745
  if (bus) {
    T::Initialize(bus);
  } else {
    T::InitializeFake();
  }
#endif
}

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_INIT_INITIALIZE_DBUS_CLIENT_H_
