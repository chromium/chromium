// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace ash {

class NetworkMetadataStore;

namespace sync_wifi {

class LocalNetworkCollectorImpl;
class SyncedNetworkUpdaterImpl;
class WifiConfigurationBridge;
class SyncedNetworkMetricsLogger;
class TimerFactory;

// A profile keyed service which instantiates and provides access to an instance
// of WifiConfigurationBridge.
class WifiConfigurationSyncService : public KeyedService {
 public:
  WifiConfigurationSyncService(
      version_info::Channel channel,
      PrefService* pref_service,
      syncer::OnceModelTypeStoreFactory create_store_callback);

  WifiConfigurationSyncService(const WifiConfigurationSyncService&) = delete;
  WifiConfigurationSyncService& operator=(const WifiConfigurationSyncService&) =
      delete;

  ~WifiConfigurationSyncService() override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate();
  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store);

 private:
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::unique_ptr<SyncedNetworkMetricsLogger> metrics_logger_;
  std::unique_ptr<TimerFactory> timer_factory_;
  std::unique_ptr<SyncedNetworkUpdaterImpl> updater_;
  std::unique_ptr<LocalNetworkCollectorImpl> collector_;
  std::unique_ptr<WifiConfigurationBridge> bridge_;
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_H_
