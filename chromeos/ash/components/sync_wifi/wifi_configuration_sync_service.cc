// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/callback_helpers.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/local_network_collector_impl.h"
#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker_impl.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/ash/components/sync_wifi/synced_network_updater_impl.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_bridge.h"
#include "chromeos/ash/components/timer_factory/timer_factory_impl.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"

namespace ash::sync_wifi {

WifiConfigurationSyncService::WifiConfigurationSyncService(
    version_info::Channel channel,
    PrefService* pref_service,
    syncer::OnceDataTypeStoreFactory create_store_callback) {
  NetworkHandler* network_handler = NetworkHandler::Get();
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  metrics_logger_ = std::make_unique<SyncedNetworkMetricsLogger>(
      network_handler->network_state_handler(),
      network_handler->network_connection_handler());
  timer_factory_ = ash::timer_factory::TimerFactoryImpl::Factory::Create();
  updater_ = std::make_unique<SyncedNetworkUpdaterImpl>(
      std::make_unique<PendingNetworkConfigurationTrackerImpl>(pref_service),
      remote_cros_network_config_.get(), timer_factory_.get(),
      metrics_logger_.get());
  collector_ = std::make_unique<LocalNetworkCollectorImpl>(
      remote_cros_network_config_.get(), metrics_logger_.get());
  bridge_ = std::make_unique<sync_wifi::WifiConfigurationBridge>(
      updater_.get(), collector_.get(),
      network_handler->network_configuration_handler(), metrics_logger_.get(),
      timer_factory_.get(), pref_service,
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::WIFI_CONFIGURATIONS,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      std::move(create_store_callback));
  NetworkMetadataStore* metadata_store =
      network_handler->network_metadata_store();
  if (metadata_store)
    SetNetworkMetadataStore(metadata_store->GetWeakPtr());
}

WifiConfigurationSyncService::~WifiConfigurationSyncService() = default;

base::WeakPtr<syncer::DataTypeControllerDelegate>
WifiConfigurationSyncService::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

void WifiConfigurationSyncService::SetNetworkMetadataStore(
    base::WeakPtr<NetworkMetadataStore> network_metadata_store) {
  bridge_->SetNetworkMetadataStore(network_metadata_store);
  collector_->SetNetworkMetadataStore(network_metadata_store);
}

}  // namespace ash::sync_wifi
