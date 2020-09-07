// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class Thread;
}  // namespace base

namespace dbus {
class Bus;
}  // namespace dbus

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
class CiceroneClient;
class ConciergeClient;
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
class SeneschalClient;
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
// DBusThreadManager manages the D-Bus thread, the thread dedicated to
// handling asynchronous D-Bus operations.
//
// This class also manages D-Bus connections and D-Bus clients, which
// depend on the D-Bus thread to ensure the right order of shutdowns for
// the D-Bus thread, the D-Bus connections, and the D-Bus clients.
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusThreadManager {
 public:
  // Processes for which to create and initialize the D-Bus clients.
  // TODO(jamescook): Move creation of clients into //ash and //chrome/browser.
  // http://crbug.com/647367
  enum ClientSet {
    // Common clients needed by both ash and the browser.
    kShared,

    // Includes the client in |kShared| as well as the clients used only by
    // the browser (and not ash).
    kAll
  };
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  // This will initialize real or fake DBusClients depending on command-line
  // arguments and whether this process runs in a ChromeOS environment.
  // Only D-Bus clients specified in |client_set| will be created.
  static void Initialize(ClientSet client_set);

  // Equivalent to Initialize(kAll).
  static void Initialize();

  // Returns a DBusThreadManagerSetter instance that allows tests to
  // replace individual D-Bus clients with their own implementations.
  // Also initializes the main DBusThreadManager for testing if necessary.
  static std::unique_ptr<DBusThreadManagerSetter> GetSetterForTesting();

  // Returns true if DBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down DBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // Returns true if clients are faked.
  bool IsUsingFakes();

  // Returns various D-Bus bus instances, owned by DBusThreadManager.
  dbus::Bus* GetSystemBus();

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
  CiceroneClient* GetCiceroneClient();
  ConciergeClient* GetConciergeClient();
  CrosDisksClient* GetCrosDisksClient();
  DebugDaemonClient* GetDebugDaemonClient();
  EasyUnlockClient* GetEasyUnlockClient();
  GnubbyClient* GetGnubbyClient();
  ImageBurnerClient* GetImageBurnerClient();
  ImageLoaderClient* GetImageLoaderClient();
  LorgnetteManagerClient* GetLorgnetteManagerClient();
  OobeConfigurationClient* GetOobeConfigurationClient();
  RuntimeProbeClient* GetRuntimeProbeClient();
  SeneschalClient* GetSeneschalClient();
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
  friend class DBusThreadManagerSetter;

  // Creates dbus clients based on |client_set|. Creates real clients if
  // |use_real_clients| is set, otherwise creates fakes.
  DBusThreadManager(ClientSet client_set, bool use_real_clients);
  ~DBusThreadManager();

  // Initializes all currently stored DBusClients with the system bus and
  // performs additional setup.
  void InitializeClients();

  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;

  // Whether to use real or fake dbus clients.
  const bool use_real_clients_;

  // Clients used only by the browser process. Null in other processes.
  std::unique_ptr<DBusClientsBrowser> clients_browser_;

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

// TODO(jamescook): Replace these with FooClient::InitializeForTesting().
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusThreadManagerSetter {
 public:
  ~DBusThreadManagerSetter();

  void SetChunneldClient(std::unique_ptr<ChunneldClient> client);
  void SetCiceroneClient(std::unique_ptr<CiceroneClient> client);
  void SetConciergeClient(std::unique_ptr<ConciergeClient> client);
  void SetCrosDisksClient(std::unique_ptr<CrosDisksClient> client);
  void SetDebugDaemonClient(std::unique_ptr<DebugDaemonClient> client);
  void SetGnubbyClient(std::unique_ptr<GnubbyClient> client);
  void SetImageBurnerClient(std::unique_ptr<ImageBurnerClient> client);
  void SetImageLoaderClient(std::unique_ptr<ImageLoaderClient> client);
  void SetSeneschalClient(std::unique_ptr<SeneschalClient> client);
  void SetRuntimeProbeClient(std::unique_ptr<RuntimeProbeClient> client);
  void SetSmbProviderClient(std::unique_ptr<SmbProviderClient> client);
  void SetUpdateEngineClient(std::unique_ptr<UpdateEngineClient> client);

 private:
  friend class DBusThreadManager;

  DBusThreadManagerSetter();

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManagerSetter);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
