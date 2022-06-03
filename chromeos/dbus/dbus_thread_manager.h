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
class AnomalyDetectorClient;
class ArcAppfuseProviderClient;
class ArcDataSnapshotdClient;
class ArcKeymasterClient;
class ArcMidisClient;
class ArcObbMounterClient;
class CecServiceClient;
class ChunneldClient;
class CrosDisksClient;
class DBusClientsBrowser;
class DBusThreadManagerSetter;
class DebugDaemonClient;
class EasyUnlockClient;
class GnubbyClient;
class ImageBurnerClient;
class ImageLoaderClient;
class LorgnetteManagerClient;
class ModemMessagingClient;
class OobeConfigurationClient;
class RuntimeProbeClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillProfileClient;
class ShillServiceClient;
class ShillThirdPartyVpnDriverClient;
class SmbProviderClient;
class SMSClient;
class UpdateEngineClient;
class VirtualFileProviderClient;
class VmPluginDispatcherClient;

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

  // Returns a DBusThreadManagerSetter instance that allows tests to replace
  // individual D-Bus clients with their own implementations. The returned
  // object will be destroyed in DBusThreadManager::Shutdown(). This method
  // can be called before calling DBusThreadManager::Initialize() which is
  // useful for browser tests, but does NOT initialize the manager itself.
  static DBusThreadManagerSetter* GetSetterForTesting();

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
  AnomalyDetectorClient* GetAnomalyDetectorClient();
  ArcAppfuseProviderClient* GetArcAppfuseProviderClient();
  ArcDataSnapshotdClient* GetArcDataSnapshotdClient();
  ArcKeymasterClient* GetArcKeymasterClient();
  ArcMidisClient* GetArcMidisClient();
  ArcObbMounterClient* GetArcObbMounterClient();
  CecServiceClient* GetCecServiceClient();
  ChunneldClient* GetChunneldClient();
  CrosDisksClient* GetCrosDisksClient();
  DebugDaemonClient* GetDebugDaemonClient();
  EasyUnlockClient* GetEasyUnlockClient();
  GnubbyClient* GetGnubbyClient();
  ImageBurnerClient* GetImageBurnerClient();
  ImageLoaderClient* GetImageLoaderClient();
  LorgnetteManagerClient* GetLorgnetteManagerClient();
  OobeConfigurationClient* GetOobeConfigurationClient();
  RuntimeProbeClient* GetRuntimeProbeClient();
  SmbProviderClient* GetSmbProviderClient();
  UpdateEngineClient* GetUpdateEngineClient();
  VirtualFileProviderClient* GetVirtualFileProviderClient();
  VmPluginDispatcherClient* GetVmPluginDispatcherClient();

  // DEPRECATED, DO NOT USE. The static getter for each of these classes should
  // be used instead. TODO(stevenjb): Remove. https://crbug.com/948390.
  ModemMessagingClient* GetModemMessagingClient();
  SMSClient* GetSMSClient();
  ShillDeviceClient* GetShillDeviceClient();
  ShillIPConfigClient* GetShillIPConfigClient();
  ShillManagerClient* GetShillManagerClient();
  ShillProfileClient* GetShillProfileClient();
  ShillServiceClient* GetShillServiceClient();
  ShillThirdPartyVpnDriverClient* GetShillThirdPartyVpnDriverClient();

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

// TODO(jamescook): Replace these with FooClient::InitializeForTesting().
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusThreadManagerSetter {
 public:
  void SetCrosDisksClient(std::unique_ptr<CrosDisksClient> client);
  void SetDebugDaemonClient(std::unique_ptr<DebugDaemonClient> client);
  void SetGnubbyClient(std::unique_ptr<GnubbyClient> client);
  void SetImageBurnerClient(std::unique_ptr<ImageBurnerClient> client);
  void SetImageLoaderClient(std::unique_ptr<ImageLoaderClient> client);
  void SetSmbProviderClient(std::unique_ptr<SmbProviderClient> client);
  void SetUpdateEngineClient(std::unique_ptr<UpdateEngineClient> client);

 private:
  friend class DBusThreadManager;

  DBusThreadManagerSetter();
  DBusThreadManagerSetter(const DBusThreadManagerSetter&) = delete;
  const DBusThreadManagerSetter& operator=(const DBusThreadManagerSetter&) =
      delete;
  ~DBusThreadManagerSetter();

  std::unique_ptr<CrosDisksClient> cros_disks_client_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<GnubbyClient> gnubby_client_;
  std::unique_ptr<ImageBurnerClient> image_burner_client_;
  std::unique_ptr<ImageLoaderClient> image_loader_client_;
  std::unique_ptr<SmbProviderClient> smb_provider_client_;
  std::unique_ptr<UpdateEngineClient> update_engine_client_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after moved to ash.
namespace ash {
using ::chromeos::DBusThreadManager;
}

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
