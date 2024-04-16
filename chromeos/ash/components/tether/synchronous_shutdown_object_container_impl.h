// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/tether/synchronous_shutdown_object_container.h"

class PrefService;

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace secure_channel {
class SecureChannelClient;
}

class NetworkConnect;
class NetworkStateHandler;

namespace tether {

class ActiveHost;
class ActiveHostNetworkStateUpdater;
class AsynchronousShutdownObjectContainer;
class ConnectionPreserver;
class NetworkConnectionHandlerTetherDelegate;
class DeviceIdTetherNetworkGuidMap;
class GmsCoreNotificationsStateTrackerImpl;
class HostScanner;
class HostScanScheduler;
class HotspotUsageDurationTracker;
class KeepAliveScheduler;
class HostConnectionMetricsLogger;
class TopLevelHostScanCache;
class NetworkHostScanCache;
class NetworkListSorter;
class NotificationPresenter;
class NotificationRemover;
class PersistentHostScanCache;
class TetherConnector;
class TetherDisconnector;
class TetherHostResponseRecorder;
class TetherNetworkDisconnectionHandler;
class TetherSessionCompletionLogger;
class WifiHotspotConnector;

// Concrete SynchronousShutdownObjectContainer implementation.
class SynchronousShutdownObjectContainerImpl
    : public SynchronousShutdownObjectContainer {
 public:
  class Factory {
   public:
    static std::unique_ptr<SynchronousShutdownObjectContainer> Create(
        AsynchronousShutdownObjectContainer* asychronous_container,
        NotificationPresenter* notification_presenter,
        GmsCoreNotificationsStateTrackerImpl*
            gms_core_notifications_state_tracker,
        PrefService* pref_service,
        NetworkHandler* network_handler,
        NetworkConnect* network_connect,
        session_manager::SessionManager* session_manager,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);
    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<SynchronousShutdownObjectContainer> CreateInstance(
        AsynchronousShutdownObjectContainer* asychronous_container,
        NotificationPresenter* notification_presenter,
        GmsCoreNotificationsStateTrackerImpl*
            gms_core_notifications_state_tracker,
        PrefService* pref_service,
        NetworkHandler* network_handler,
        NetworkConnect* network_connect,
        session_manager::SessionManager* session_manager,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client) = 0;
    virtual ~Factory();

   private:
    static Factory* factory_instance_;
  };

  SynchronousShutdownObjectContainerImpl(
      const SynchronousShutdownObjectContainerImpl&) = delete;
  SynchronousShutdownObjectContainerImpl& operator=(
      const SynchronousShutdownObjectContainerImpl&) = delete;

  ~SynchronousShutdownObjectContainerImpl() override;

  // SynchronousShutdownObjectContainer:
  ActiveHost* active_host() override;
  HostScanCache* host_scan_cache() override;
  HostScanScheduler* host_scan_scheduler() override;
  TetherDisconnector* tether_disconnector() override;

 protected:
  SynchronousShutdownObjectContainerImpl(
      AsynchronousShutdownObjectContainer* asychronous_container,
      NotificationPresenter* notification_presenter,
      GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      PrefService* pref_service,
      NetworkHandler* network_handler,
      NetworkConnect* network_connect,
      session_manager::SessionManager* session_manager,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

 private:
  raw_ptr<NetworkStateHandler> network_state_handler_;

  std::unique_ptr<NetworkListSorter> network_list_sorter_;
  std::unique_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<TetherSessionCompletionLogger>
      tether_session_completion_logger_;
  std::unique_ptr<WifiHotspotConnector> wifi_hotspot_connector_;
  std::unique_ptr<ActiveHost> active_host_;
  std::unique_ptr<ActiveHostNetworkStateUpdater>
      active_host_network_state_updater_;
  std::unique_ptr<PersistentHostScanCache> persistent_host_scan_cache_;
  std::unique_ptr<NetworkHostScanCache> network_host_scan_cache_;
  std::unique_ptr<TopLevelHostScanCache> top_level_host_scan_cache_;
  std::unique_ptr<NotificationRemover> notification_remover_;
  std::unique_ptr<KeepAliveScheduler> keep_alive_scheduler_;
  std::unique_ptr<HotspotUsageDurationTracker> hotspot_usage_duration_tracker_;
  std::unique_ptr<ConnectionPreserver> connection_preserver_;
  std::unique_ptr<HostScanner> host_scanner_;
  std::unique_ptr<HostScanScheduler> host_scan_scheduler_;
  std::unique_ptr<HostConnectionMetricsLogger> host_connection_metrics_logger_;
  std::unique_ptr<TetherConnector> tether_connector_;
  std::unique_ptr<TetherDisconnector> tether_disconnector_;
  std::unique_ptr<TetherNetworkDisconnectionHandler>
      tether_network_disconnection_handler_;
  std::unique_ptr<NetworkConnectionHandlerTetherDelegate>
      network_connection_handler_tether_delegate_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_
