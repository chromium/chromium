// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_DBUS_LACROS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_LACROS_DBUS_LACROS_DBUS_THREAD_MANAGER_H_

#include "base/component_export.h"
#include "chromeos/dbus/init/dbus_thread_manager_base.h"

namespace chromeos {

// Lacros implementation of DBusThreadManagerBase.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosDBusThreadManager
    : public DBusThreadManagerBase {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  // This will initialize real or fake DBusClients depending on command-line
  // arguments and whether this process runs in a ChromeOS environment.
  static void Initialize();

  // Gets the global instance. Initialize() must be called first.
  static LacrosDBusThreadManager* Get();

  // Returns true if LacrosDBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down LacrosDBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

 private:
  LacrosDBusThreadManager();
  LacrosDBusThreadManager(const LacrosDBusThreadManager&) = delete;
  const LacrosDBusThreadManager& operator=(const LacrosDBusThreadManager&) =
      delete;
  ~LacrosDBusThreadManager() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_DBUS_LACROS_DBUS_THREAD_MANAGER_H_
