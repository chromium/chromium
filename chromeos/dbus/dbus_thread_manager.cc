// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/anomaly_detector_client.h"
#include "chromeos/dbus/arc/arc_data_snapshotd_client.h"
#include "chromeos/dbus/arc/arc_keymaster_client.h"
#include "chromeos/dbus/arc/arc_midis_client.h"
#include "chromeos/dbus/arc/arc_obb_mounter_client.h"
#include "chromeos/dbus/cec_service_client.h"
#include "chromeos/dbus/chunneld_client.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_clients_browser.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/gnubby_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/image_loader_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/runtime_probe_client.h"
#include "chromeos/dbus/seneschal_client.h"
#include "chromeos/dbus/shill/modem_messaging_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/shill/sms_client.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = nullptr;
static bool g_using_dbus_thread_manager_for_testing = false;

DBusThreadManager::DBusThreadManager(ClientSet client_set,
                                     bool use_real_clients)
    : use_real_clients_(use_real_clients) {
  if (client_set == DBusThreadManager::kAll)
    clients_browser_.reset(new DBusClientsBrowser(use_real_clients));
  // NOTE: When there are clients only used by ash, create them here.

  dbus::statistics::Initialize();

  if (use_real_clients) {
    // Create the D-Bus thread.
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    dbus_thread_.reset(new base::Thread("D-Bus thread"));
    dbus_thread_->StartWithOptions(thread_options);

    // Create the connection to the system bus.
    dbus::Bus::Options system_bus_options;
    system_bus_options.bus_type = dbus::Bus::SYSTEM;
    system_bus_options.connection_type = dbus::Bus::PRIVATE;
    system_bus_options.dbus_task_runner = dbus_thread_->task_runner();
    system_bus_ = new dbus::Bus(system_bus_options);
  }
}

DBusThreadManager::~DBusThreadManager() {
  // Delete all D-Bus clients before shutting down the system bus.
  clients_browser_.reset();

  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  if (system_bus_.get())
    system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  if (dbus_thread_)
    dbus_thread_->Stop();

  dbus::statistics::Shutdown();

  if (!g_dbus_thread_manager)
    return;  // Called form Shutdown() or local test instance.

  // There should never be both a global instance and a local instance.
  CHECK_EQ(this, g_dbus_thread_manager);
  if (g_using_dbus_thread_manager_for_testing) {
    g_dbus_thread_manager = nullptr;
    g_using_dbus_thread_manager_for_testing = false;
    VLOG(1) << "DBusThreadManager destroyed";
  } else {
    LOG(FATAL) << "~DBusThreadManager() called outside of Shutdown()";
  }
}

dbus::Bus* DBusThreadManager::GetSystemBus() {
  return system_bus_.get();
}

AnomalyDetectorClient* DBusThreadManager::GetAnomalyDetectorClient() {
  return clients_browser_ ? clients_browser_->anomaly_detector_client_.get()
                          : nullptr;
}

ArcAppfuseProviderClient* DBusThreadManager::GetArcAppfuseProviderClient() {
  return clients_browser_ ? clients_browser_->arc_appfuse_provider_client_.get()
                          : nullptr;
}

ArcDataSnapshotdClient* DBusThreadManager::GetArcDataSnapshotdClient() {
  return clients_browser_ ? clients_browser_->arc_data_snapshotd_client_.get()
                          : nullptr;
}

ArcKeymasterClient* DBusThreadManager::GetArcKeymasterClient() {
  return clients_browser_ ? clients_browser_->arc_keymaster_client_.get()
                          : nullptr;
}

ArcMidisClient* DBusThreadManager::GetArcMidisClient() {
  return clients_browser_ ? clients_browser_->arc_midis_client_.get() : nullptr;
}

ArcObbMounterClient* DBusThreadManager::GetArcObbMounterClient() {
  return clients_browser_ ? clients_browser_->arc_obb_mounter_client_.get()
                          : nullptr;
}

CecServiceClient* DBusThreadManager::GetCecServiceClient() {
  return clients_browser_ ? clients_browser_->cec_service_client_.get()
                          : nullptr;
}

ChunneldClient* DBusThreadManager::GetChunneldClient() {
  return clients_browser_ ? clients_browser_->chunneld_client_.get() : nullptr;
}

CiceroneClient* DBusThreadManager::GetCiceroneClient() {
  return clients_browser_ ? clients_browser_->cicerone_client_.get() : nullptr;
}

ConciergeClient* DBusThreadManager::GetConciergeClient() {
  return clients_browser_ ? clients_browser_->concierge_client_.get() : nullptr;
}

CrosDisksClient* DBusThreadManager::GetCrosDisksClient() {
  return clients_browser_ ? clients_browser_->cros_disks_client_.get()
                          : nullptr;
}

DebugDaemonClient* DBusThreadManager::GetDebugDaemonClient() {
  return clients_browser_ ? clients_browser_->debug_daemon_client_.get()
                          : nullptr;
}

EasyUnlockClient* DBusThreadManager::GetEasyUnlockClient() {
  return clients_browser_ ? clients_browser_->easy_unlock_client_.get()
                          : nullptr;
}

GnubbyClient* DBusThreadManager::GetGnubbyClient() {
  return clients_browser_ ? clients_browser_->gnubby_client_.get() : nullptr;
}

ShillDeviceClient* DBusThreadManager::GetShillDeviceClient() {
  return ShillDeviceClient::Get();
}

ShillIPConfigClient* DBusThreadManager::GetShillIPConfigClient() {
  return ShillIPConfigClient::Get();
}

ShillManagerClient* DBusThreadManager::GetShillManagerClient() {
  return ShillManagerClient::Get();
}

ShillServiceClient* DBusThreadManager::GetShillServiceClient() {
  return ShillServiceClient::Get();
}

ShillProfileClient* DBusThreadManager::GetShillProfileClient() {
  return ShillProfileClient::Get();
}

ShillThirdPartyVpnDriverClient*
DBusThreadManager::GetShillThirdPartyVpnDriverClient() {
  return ShillThirdPartyVpnDriverClient::Get();
}

ImageBurnerClient* DBusThreadManager::GetImageBurnerClient() {
  return clients_browser_ ? clients_browser_->image_burner_client_.get()
                          : nullptr;
}

ImageLoaderClient* DBusThreadManager::GetImageLoaderClient() {
  return clients_browser_ ? clients_browser_->image_loader_client_.get()
                          : nullptr;
}

LorgnetteManagerClient* DBusThreadManager::GetLorgnetteManagerClient() {
  return clients_browser_ ? clients_browser_->lorgnette_manager_client_.get()
                          : nullptr;
}

ModemMessagingClient* DBusThreadManager::GetModemMessagingClient() {
  return ModemMessagingClient::Get();
}

OobeConfigurationClient* DBusThreadManager::GetOobeConfigurationClient() {
  return clients_browser_->oobe_configuration_client_.get();
}

RuntimeProbeClient* DBusThreadManager::GetRuntimeProbeClient() {
  return clients_browser_ ? clients_browser_->runtime_probe_client_.get()
                          : nullptr;
}

SeneschalClient* DBusThreadManager::GetSeneschalClient() {
  return clients_browser_ ? clients_browser_->seneschal_client_.get() : nullptr;
}

SmbProviderClient* DBusThreadManager::GetSmbProviderClient() {
  return clients_browser_ ? clients_browser_->smb_provider_client_.get()
                          : nullptr;
}

SMSClient* DBusThreadManager::GetSMSClient() {
  return SMSClient::Get();
}

UpdateEngineClient* DBusThreadManager::GetUpdateEngineClient() {
  return clients_browser_ ? clients_browser_->update_engine_client_.get()
                          : nullptr;
}

VirtualFileProviderClient* DBusThreadManager::GetVirtualFileProviderClient() {
  return clients_browser_
             ? clients_browser_->virtual_file_provider_client_.get()
             : nullptr;
}

VmPluginDispatcherClient* DBusThreadManager::GetVmPluginDispatcherClient() {
  return clients_browser_ ? clients_browser_->vm_plugin_dispatcher_client_.get()
                          : nullptr;
}

void DBusThreadManager::InitializeClients() {
  // Some clients call DBusThreadManager::Get() during initialization.
  DCHECK(g_dbus_thread_manager);

  // TODO(stevenjb): Move these to dbus_helper.cc in src/chrome and any tests
  // that require Shill clients. https://crbug.com/948390.
  shill_clients::Initialize(GetSystemBus());

  if (clients_browser_)
    clients_browser_->Initialize(GetSystemBus());

  if (use_real_clients_)
    VLOG(1) << "DBusThreadManager initialized for Chrome OS";
  else
    VLOG(1) << "DBusThreadManager created for testing";
}

bool DBusThreadManager::IsUsingFakes() {
  return !use_real_clients_;
}

// static
void DBusThreadManager::Initialize(ClientSet client_set) {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  if (g_using_dbus_thread_manager_for_testing)
    return;

  CHECK(!g_dbus_thread_manager);
#if defined(USE_REAL_DBUS_CLIENTS)
  bool use_real_clients = true;
#else
  // TODO(hashimoto): Always use fakes after adding
  // use_real_dbus_clients=true to where needed. crbug.com/952745
  bool use_real_clients = base::SysInfo::IsRunningOnChromeOS() &&
                          !base::CommandLine::ForCurrentProcess()->HasSwitch(
                              chromeos::switches::kDbusStub);
#endif
  g_dbus_thread_manager = new DBusThreadManager(client_set, use_real_clients);
  g_dbus_thread_manager->InitializeClients();
}

// static
void DBusThreadManager::Initialize() {
  Initialize(kAll);
}

// static
std::unique_ptr<DBusThreadManagerSetter>
DBusThreadManager::GetSetterForTesting() {
  if (!g_using_dbus_thread_manager_for_testing) {
    g_using_dbus_thread_manager_for_testing = true;
    CHECK(!g_dbus_thread_manager);
    // TODO(jamescook): Don't initialize clients as a side-effect of using a
    // test API. For now, assume the caller wants all clients.
    g_dbus_thread_manager =
        new DBusThreadManager(kAll, false /* use_real_clients */);
    g_dbus_thread_manager->InitializeClients();
  }

  return base::WrapUnique(new DBusThreadManagerSetter());
}

// static
bool DBusThreadManager::IsInitialized() {
  return !!g_dbus_thread_manager;
}

// static
void DBusThreadManager::Shutdown() {
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_dbus_thread_manager);

  // TODO(stevenjb): Remove. https://crbug.com/948390.
  shill_clients::Shutdown();

  DBusThreadManager* dbus_thread_manager = g_dbus_thread_manager;
  g_dbus_thread_manager = nullptr;
  g_using_dbus_thread_manager_for_testing = false;
  delete dbus_thread_manager;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

DBusThreadManagerSetter::DBusThreadManagerSetter() = default;

DBusThreadManagerSetter::~DBusThreadManagerSetter() = default;

void DBusThreadManagerSetter::SetChunneldClient(
    std::unique_ptr<ChunneldClient> client) {
  DBusThreadManager::Get()->clients_browser_->chunneld_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetCiceroneClient(
    std::unique_ptr<CiceroneClient> client) {
  DBusThreadManager::Get()->clients_browser_->cicerone_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetConciergeClient(
    std::unique_ptr<ConciergeClient> client) {
  DBusThreadManager::Get()->clients_browser_->concierge_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetCrosDisksClient(
    std::unique_ptr<CrosDisksClient> client) {
  DBusThreadManager::Get()->clients_browser_->cros_disks_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetDebugDaemonClient(
    std::unique_ptr<DebugDaemonClient> client) {
  DBusThreadManager::Get()->clients_browser_->debug_daemon_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetGnubbyClient(
    std::unique_ptr<GnubbyClient> client) {
  DBusThreadManager::Get()->clients_browser_->gnubby_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetRuntimeProbeClient(
    std::unique_ptr<RuntimeProbeClient> client) {
  DBusThreadManager::Get()->clients_browser_->runtime_probe_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetSeneschalClient(
    std::unique_ptr<SeneschalClient> client) {
  DBusThreadManager::Get()->clients_browser_->seneschal_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetImageBurnerClient(
    std::unique_ptr<ImageBurnerClient> client) {
  DBusThreadManager::Get()->clients_browser_->image_burner_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetImageLoaderClient(
    std::unique_ptr<ImageLoaderClient> client) {
  DBusThreadManager::Get()->clients_browser_->image_loader_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetSmbProviderClient(
    std::unique_ptr<SmbProviderClient> client) {
  DBusThreadManager::Get()->clients_browser_->smb_provider_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetUpdateEngineClient(
    std::unique_ptr<UpdateEngineClient> client) {
  DBusThreadManager::Get()->clients_browser_->update_engine_client_ =
      std::move(client);
}

}  // namespace chromeos
