// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_clients_browser.h"

#include "base/check.h"
#include "chromeos/dbus/anomaly_detector_client.h"
#include "chromeos/dbus/arc/arc_appfuse_provider_client.h"
#include "chromeos/dbus/arc/arc_data_snapshotd_client.h"
#include "chromeos/dbus/arc/arc_keymaster_client.h"
#include "chromeos/dbus/arc/arc_midis_client.h"
#include "chromeos/dbus/arc/arc_obb_mounter_client.h"
#include "chromeos/dbus/arc/fake_arc_appfuse_provider_client.h"
#include "chromeos/dbus/arc/fake_arc_data_snapshotd_client.h"
#include "chromeos/dbus/arc/fake_arc_keymaster_client.h"
#include "chromeos/dbus/arc/fake_arc_midis_client.h"
#include "chromeos/dbus/arc/fake_arc_obb_mounter_client.h"
#include "chromeos/dbus/cec_service_client.h"
#include "chromeos/dbus/chunneld_client.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/fake_anomaly_detector_client.h"
#include "chromeos/dbus/fake_cec_service_client.h"
#include "chromeos/dbus/fake_chunneld_client.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "chromeos/dbus/fake_gnubby_client.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/fake_oobe_configuration_client.h"
#include "chromeos/dbus/fake_runtime_probe_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/fake_smb_provider_client.h"
#include "chromeos/dbus/fake_virtual_file_provider_client.h"
#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"
#include "chromeos/dbus/gnubby_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/image_loader_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/oobe_configuration_client.h"
#include "chromeos/dbus/runtime_probe_client.h"
#include "chromeos/dbus/seneschal_client.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/dbus/virtual_file_provider_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"

namespace chromeos {

// CREATE_DBUS_CLIENT creates the appropriate version of D-Bus client.
#if defined(USE_REAL_DBUS_CLIENTS)
// Create the real D-Bus client. use_real_clients is ignored.
#define CREATE_DBUS_CLIENT(type, use_real_clients) type::Create()
#else
// Create a fake if use_real_clients == false.
// TODO(hashimoto): Always use fakes after adding
// use_real_dbus_clients=true to where needed. crbug.com/952745
#define CREATE_DBUS_CLIENT(type, use_real_clients) \
  (use_real_clients ? type::Create() : std::make_unique<Fake##type>())
#endif  // USE_REAL_DBUS_CLIENTS

DBusClientsBrowser::DBusClientsBrowser(bool use_real_clients) {
  // TODO(hashimoto): Use CREATE_DBUS_CLIENT for all clients after removing
  // DBusClientImplementationType and converting all Create() methods to return
  // std::unique_ptr. crbug.com/952745
  const DBusClientImplementationType client_impl_type =
      use_real_clients ? REAL_DBUS_CLIENT_IMPLEMENTATION
                       : FAKE_DBUS_CLIENT_IMPLEMENTATION;

  anomaly_detector_client_ =
      CREATE_DBUS_CLIENT(AnomalyDetectorClient, use_real_clients);
  arc_appfuse_provider_client_ =
      CREATE_DBUS_CLIENT(ArcAppfuseProviderClient, use_real_clients);
  arc_data_snapshotd_client_ =
      CREATE_DBUS_CLIENT(ArcDataSnapshotdClient, use_real_clients);
  arc_keymaster_client_ =
      CREATE_DBUS_CLIENT(ArcKeymasterClient, use_real_clients);
  arc_midis_client_ = CREATE_DBUS_CLIENT(ArcMidisClient, use_real_clients);
  arc_obb_mounter_client_ =
      CREATE_DBUS_CLIENT(ArcObbMounterClient, use_real_clients);
  cec_service_client_ = CREATE_DBUS_CLIENT(CecServiceClient, use_real_clients);
  cros_disks_client_ = CREATE_DBUS_CLIENT(CrosDisksClient, use_real_clients);
  chunneld_client_ = CREATE_DBUS_CLIENT(ChunneldClient, use_real_clients);
  cicerone_client_ = CREATE_DBUS_CLIENT(CiceroneClient, use_real_clients);
  concierge_client_ = CREATE_DBUS_CLIENT(ConciergeClient, use_real_clients);
  debug_daemon_client_ =
      CREATE_DBUS_CLIENT(DebugDaemonClient, use_real_clients);
  easy_unlock_client_ = CREATE_DBUS_CLIENT(EasyUnlockClient, use_real_clients);
  gnubby_client_ = CREATE_DBUS_CLIENT(GnubbyClient, use_real_clients);
  image_burner_client_ =
      CREATE_DBUS_CLIENT(ImageBurnerClient, use_real_clients);
  image_loader_client_ =
      CREATE_DBUS_CLIENT(ImageLoaderClient, use_real_clients);
  lorgnette_manager_client_ =
      CREATE_DBUS_CLIENT(LorgnetteManagerClient, use_real_clients);
  oobe_configuration_client_ =
      CREATE_DBUS_CLIENT(OobeConfigurationClient, use_real_clients);
  runtime_probe_client_ =
      CREATE_DBUS_CLIENT(RuntimeProbeClient, use_real_clients);
  seneschal_client_ = CREATE_DBUS_CLIENT(SeneschalClient, use_real_clients);
  smb_provider_client_ =
      CREATE_DBUS_CLIENT(SmbProviderClient, use_real_clients);
  update_engine_client_.reset(UpdateEngineClient::Create(client_impl_type));
  virtual_file_provider_client_ =
      CREATE_DBUS_CLIENT(VirtualFileProviderClient, use_real_clients);
  vm_plugin_dispatcher_client_ =
      CREATE_DBUS_CLIENT(VmPluginDispatcherClient, use_real_clients);
}

DBusClientsBrowser::~DBusClientsBrowser() = default;

void DBusClientsBrowser::Initialize(dbus::Bus* system_bus) {
  DCHECK(DBusThreadManager::IsInitialized());

  anomaly_detector_client_->Init(system_bus);
  arc_appfuse_provider_client_->Init(system_bus);
  arc_data_snapshotd_client_->Init(system_bus);
  arc_keymaster_client_->Init(system_bus);
  arc_midis_client_->Init(system_bus);
  arc_obb_mounter_client_->Init(system_bus);
  cec_service_client_->Init(system_bus);
  chunneld_client_->Init(system_bus);
  cicerone_client_->Init(system_bus);
  concierge_client_->Init(system_bus);
  cros_disks_client_->Init(system_bus);
  debug_daemon_client_->Init(system_bus);
  easy_unlock_client_->Init(system_bus);
  gnubby_client_->Init(system_bus);
  image_burner_client_->Init(system_bus);
  image_loader_client_->Init(system_bus);
  lorgnette_manager_client_->Init(system_bus);
  oobe_configuration_client_->Init(system_bus);
  runtime_probe_client_->Init(system_bus);
  seneschal_client_->Init(system_bus);
  smb_provider_client_->Init(system_bus);
  update_engine_client_->Init(system_bus);
  virtual_file_provider_client_->Init(system_bus);
  vm_plugin_dispatcher_client_->Init(system_bus);
}

}  // namespace chromeos
