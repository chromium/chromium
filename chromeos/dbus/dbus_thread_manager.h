// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include "base/component_export.h"
#include "chromeos/dbus/init/dbus_thread_manager_base.h"

namespace chromeos {

// Ash implementation of DBusThreadManagerBase.
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusThreadManager
    : public DBusThreadManagerBase {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  static void Initialize();

  // Returns true if DBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down DBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

 private:
  DBusThreadManager();
  DBusThreadManager(const DBusThreadManager&) = delete;
  const DBusThreadManager& operator=(const DBusThreadManager&) = delete;
  ~DBusThreadManager() override;

  // Performs additional setup.
  // TODO(https://crbug.com/948390): Remove after shill client initialization
  // moves into chrome/browser.
  void InitializeClients();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after moved to ash.
namespace ash {
using ::chromeos::DBusThreadManager;
}

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
