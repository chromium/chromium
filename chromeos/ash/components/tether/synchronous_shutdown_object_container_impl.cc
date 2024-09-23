// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/synchronous_shutdown_object_container_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/active_host_network_state_updater.h"
#include "chromeos/ash/components/tether/asynchronous_shutdown_object_container.h"
#include "chromeos/ash/components/tether/connection_preserver_impl.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/host_connection_metrics_logger.h"
#include "chromeos/ash/components/tether/host_scan_scheduler_impl.h"
#include "chromeos/ash/components/tether/host_scanner_impl.h"
#include "chromeos/ash/components/tether/hotspot_usage_duration_tracker.h"
#include "chromeos/ash/components/tether/keep_alive_scheduler.h"
#include "chromeos/ash/components/tether/network_connection_handler_tether_delegate.h"
#include "chromeos/ash/components/tether/network_host_scan_cache.h"
#include "chromeos/ash/components/tether/network_list_sorter.h"
#include "chromeos/ash/components/tether/notification_remover.h"
#include "chromeos/ash/components/tether/persistent_host_scan_cache_impl.h"
#include "chromeos/ash/components/tether/secure_channel_tether_availability_operation_orchestrator.h"
#include "chromeos/ash/components/tether/tether_connector_impl.h"
#include "chromeos/ash/components/tether/tether_disconnector_impl.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"
#include "chromeos/ash/components/tether/tether_network_disconnection_handler.h"
#include "chromeos/ash/components/tether/tether_session_completion_logger.h"
#include "chromeos/ash/components/tether/top_level_host_scan_cache.h"
#include "chromeos/ash/components/tether/wifi_hotspot_connector.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"
#include "chromeos/ash/components/timer_factory/timer_factory_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash::tether {

// static
SynchronousShutdownObjectContainerImpl::Factory*
    SynchronousShutdownObjectContainerImpl::Factory::factory_instance_ =
        nullptr;

// static
std::unique_ptr<SynchronousShutdownObjectContainer>
SynchronousShutdownObjectContainerImpl::Factory::Create(
    AsynchronousShutdownObjectContainer* asychronous_container,
    NotificationPresenter* notification_presenter,
    GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker,
    PrefService* pref_service,
    NetworkHandler* network_handler,
    NetworkConnect* network_connect,
    session_manager::SessionManager* session_manager,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        asychronous_container, notification_presenter,
        gms_core_notifications_state_tracker, pref_service, network_handler,
        network_connect, session_manager, device_sync_client,
        secure_channel_client);
  }

  return base::WrapUnique(new SynchronousShutdownObjectContainerImpl(
      asychronous_container, notification_presenter,
      gms_core_notifications_state_tracker, pref_service, network_handler,
      network_connect, session_manager, device_sync_client,
      secure_channel_client));
}

// static
void SynchronousShutdownObjectContainerImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

SynchronousShutdownObjectContainerImpl::Factory::~Factory() = default;

SynchronousShutdownObjectContainerImpl::SynchronousShutdownObjectContainerImpl(
    AsynchronousShutdownObjectContainer* asychronous_container,
    NotificationPresenter* notification_presenter,
    GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker,
    PrefService* pref_service,
    NetworkHandler* network_handler,
    NetworkConnect* network_connect,
    session_manager::SessionManager* session_manager,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : network_state_handler_(network_handler->network_state_handler()),
      network_list_sorter_(std::make_unique<NetworkListSorter>()),
      tether_host_response_recorder_(
          std::make_unique<TetherHostResponseRecorder>(pref_service)),
      device_id_tether_network_guid_map_(
          std::make_unique<DeviceIdTetherNetworkGuidMap>()),
      tether_session_completion_logger_(
          std::make_unique<TetherSessionCompletionLogger>()),
      wifi_hotspot_connector_(
          std::make_unique<WifiHotspotConnector>(network_handler,
                                                 network_connect)),
      active_host_(std::make_unique<ActiveHost>(
          asychronous_container->tether_host_fetcher(),
          pref_service)),
      active_host_network_state_updater_(
          std::make_unique<ActiveHostNetworkStateUpdater>(
              active_host_.get(),
              network_state_handler_)),
      persistent_host_scan_cache_(
          std::make_unique<PersistentHostScanCacheImpl>(pref_service)),
      network_host_scan_cache_(std::make_unique<NetworkHostScanCache>(
          network_state_handler_,
          tether_host_response_recorder_.get(),
          device_id_tether_network_guid_map_.get())),
      top_level_host_scan_cache_(std::make_unique<TopLevelHostScanCache>(
          ash::timer_factory::TimerFactoryImpl::Factory::Create(),
          active_host_.get(),
          network_host_scan_cache_.get(),
          persistent_host_scan_cache_.get())),
      notification_remover_(std::make_unique<NotificationRemover>(
          network_state_handler_,
          notification_presenter,
          top_level_host_scan_cache_.get(),
          active_host_.get())),
      keep_alive_scheduler_(std::make_unique<KeepAliveScheduler>(
          asychronous_container->host_connection_factory(),
          active_host_.get(),
          top_level_host_scan_cache_.get(),
          device_id_tether_network_guid_map_.get())),
      hotspot_usage_duration_tracker_(
          std::make_unique<HotspotUsageDurationTracker>(
              active_host_.get(),
              base::DefaultClock::GetInstance())),
      connection_preserver_(std::make_unique<ConnectionPreserverImpl>(
          asychronous_container->host_connection_factory(),
          network_state_handler_,
          active_host_.get(),
          tether_host_response_recorder_.get())),
      host_scanner_(std::make_unique<HostScannerImpl>(
          std::make_unique<
              SecureChannelTetherAvailabilityOperationOrchestrator::Factory>(
              asychronous_container->tether_host_fetcher(),
              device_sync_client,
              asychronous_container->host_connection_factory(),
              tether_host_response_recorder_.get(),
              connection_preserver_.get()),
          network_state_handler_,
          session_manager,
          gms_core_notifications_state_tracker,
          notification_presenter,
          device_id_tether_network_guid_map_.get(),
          top_level_host_scan_cache_.get(),
          base::DefaultClock::GetInstance())),
      host_scan_scheduler_(
          std::make_unique<HostScanSchedulerImpl>(network_state_handler_,
                                                  host_scanner_.get(),
                                                  session_manager)),
      host_connection_metrics_logger_(
          std::make_unique<HostConnectionMetricsLogger>(active_host_.get())),
      tether_connector_(std::make_unique<TetherConnectorImpl>(
          asychronous_container->host_connection_factory(),
          network_state_handler_,
          wifi_hotspot_connector_.get(),
          active_host_.get(),
          asychronous_container->tether_host_fetcher(),
          tether_host_response_recorder_.get(),
          device_id_tether_network_guid_map_.get(),
          top_level_host_scan_cache_.get(),
          notification_presenter,
          host_connection_metrics_logger_.get(),
          asychronous_container->disconnect_tethering_request_sender(),
          asychronous_container->wifi_hotspot_disconnector())),
      tether_disconnector_(std::make_unique<TetherDisconnectorImpl>(
          active_host_.get(),
          asychronous_container->wifi_hotspot_disconnector(),
          asychronous_container->disconnect_tethering_request_sender(),
          tether_connector_.get(),
          device_id_tether_network_guid_map_.get(),
          tether_session_completion_logger_.get())),
      tether_network_disconnection_handler_(
          std::make_unique<TetherNetworkDisconnectionHandler>(
              active_host_.get(),
              network_state_handler_,
              asychronous_container->network_configuration_remover(),
              asychronous_container->disconnect_tethering_request_sender(),
              tether_session_completion_logger_.get())),
      network_connection_handler_tether_delegate_(
          std::make_unique<NetworkConnectionHandlerTetherDelegate>(
              network_handler->network_connection_handler(),
              active_host_.get(),
              tether_connector_.get(),
              tether_disconnector_.get())) {
  network_state_handler_->set_tether_sort_delegate(network_list_sorter_.get());
}

SynchronousShutdownObjectContainerImpl::
    ~SynchronousShutdownObjectContainerImpl() {
  network_state_handler_->set_tether_sort_delegate(nullptr);
}

ActiveHost* SynchronousShutdownObjectContainerImpl::active_host() {
  return active_host_.get();
}

HostScanCache* SynchronousShutdownObjectContainerImpl::host_scan_cache() {
  return top_level_host_scan_cache_.get();
}

HostScanScheduler*
SynchronousShutdownObjectContainerImpl::host_scan_scheduler() {
  return host_scan_scheduler_.get();
}

TetherDisconnector*
SynchronousShutdownObjectContainerImpl::tether_disconnector() {
  return tether_disconnector_.get();
}

}  // namespace ash::tether
