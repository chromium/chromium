// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/init/dbus_thread_manager_base.h"

namespace chromeos {

// Style Note: Clients are sorted by names.
class DBusClientsBrowser;
class DebugDaemonClient;
class EasyUnlockClient;

// THIS CLASS IS BEING DEPRECATED. See README.md for guidelines and
// https://crbug.com/647367 for details.
//
// Ash implementation of DBusThreadManagerBase.
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusThreadManager
    : public DBusThreadManagerBase {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  // This will initialize real or fake DBusClients depending on command-line
  // arguments and whether this process runs in a ChromeOS environment.
  static void Initialize();

  // Returns true if DBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down DBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // All returned objects are owned by DBusThreadManager.  Do not use these
  // pointers after DBusThreadManager has been shut down.
  // TODO(jamescook): Replace this with calls to FooClient::Get().
  // http://crbug.com/647367
  DebugDaemonClient* GetDebugDaemonClient();
  EasyUnlockClient* GetEasyUnlockClient();

 private:
  DBusThreadManager();
  DBusThreadManager(const DBusThreadManager&) = delete;
  const DBusThreadManager& operator=(const DBusThreadManager&) = delete;
  ~DBusThreadManager() override;

  // Initializes all currently stored DBusClients with the system bus and
  // performs additional setup.
  void InitializeClients();

  // Owns the clients.
  std::unique_ptr<DBusClientsBrowser> clients_browser_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after moved to ash.
namespace ash {
using ::chromeos::DBusThreadManager;
}

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
