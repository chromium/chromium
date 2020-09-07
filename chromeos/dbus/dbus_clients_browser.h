// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_
#define CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"

namespace dbus {
class Bus;
}

namespace chromeos {

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
class DebugDaemonClient;
class EasyUnlockClient;
class GnubbyClient;
class ImageBurnerClient;
class ImageLoaderClient;
class LorgnetteManagerClient;
class OobeConfigurationClient;
class RuntimeProbeClient;
class SeneschalClient;
class SmbProviderClient;
class UpdateEngineClient;
class VirtualFileProviderClient;
class VmPluginDispatcherClient;

// D-Bus clients used only in the browser process.
// TODO(jamescook): Move this under //chrome/browser. http://crbug.com/647367
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusClientsBrowser {
 public:
  // Creates real implementations if |use_real_clients| is true and fakes
  // otherwise. Fakes are used when running on Linux desktop and in tests.
  explicit DBusClientsBrowser(bool use_real_clients);
  ~DBusClientsBrowser();

  void Initialize(dbus::Bus* system_bus);

 private:
  friend class DBusThreadManager;
  friend class DBusThreadManagerSetter;

  std::unique_ptr<AnomalyDetectorClient> anomaly_detector_client_;
  std::unique_ptr<ArcAppfuseProviderClient> arc_appfuse_provider_client_;
  std::unique_ptr<ArcDataSnapshotdClient> arc_data_snapshotd_client_;
  std::unique_ptr<ArcKeymasterClient> arc_keymaster_client_;
  std::unique_ptr<ArcMidisClient> arc_midis_client_;
  std::unique_ptr<ArcObbMounterClient> arc_obb_mounter_client_;
  std::unique_ptr<CecServiceClient> cec_service_client_;
  std::unique_ptr<ChunneldClient> chunneld_client_;
  std::unique_ptr<CiceroneClient> cicerone_client_;
  std::unique_ptr<ConciergeClient> concierge_client_;
  std::unique_ptr<CrosDisksClient> cros_disks_client_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<EasyUnlockClient> easy_unlock_client_;
  std::unique_ptr<GnubbyClient> gnubby_client_;
  std::unique_ptr<ImageBurnerClient> image_burner_client_;
  std::unique_ptr<ImageLoaderClient> image_loader_client_;
  std::unique_ptr<LorgnetteManagerClient> lorgnette_manager_client_;
  std::unique_ptr<OobeConfigurationClient> oobe_configuration_client_;
  std::unique_ptr<RuntimeProbeClient> runtime_probe_client_;
  std::unique_ptr<SeneschalClient> seneschal_client_;
  std::unique_ptr<SmbProviderClient> smb_provider_client_;
  std::unique_ptr<UpdateEngineClient> update_engine_client_;
  std::unique_ptr<VirtualFileProviderClient> virtual_file_provider_client_;
  std::unique_ptr<VmPluginDispatcherClient> vm_plugin_dispatcher_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusClientsBrowser);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_
