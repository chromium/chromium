// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_IMPL_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/components/tether/host_scanner_operation.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/network_state_handler.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace tether {

class ConnectionPreserver;
class DeviceIdTetherNetworkGuidMap;
class GmsCoreNotificationsStateTrackerImpl;
class HostScanCache;
class HostScanDevicePrioritizer;
class TetherHostFetcher;
class TetherHostResponseRecorder;

// Scans for nearby tether hosts. When StartScan() is called, this class creates
// a new HostScannerOperation and uses it to contact nearby devices to query
// whether they can provide tether capabilities. Once the scan results are
// received, they are stored in the HostScanCache passed to the constructor,
// and observers are notified via HostScanner::Observer::ScanFinished().
class HostScannerImpl : public HostScanner,
                        public HostScannerOperation::Observer,
                        public session_manager::SessionManagerObserver {
 public:
  class Observer {
   public:
    void virtual ScanFinished() = 0;
  };

  HostScannerImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      NetworkStateHandler* network_state_handler,
      session_manager::SessionManager* session_manager,
      TetherHostFetcher* tether_host_fetcher,
      HostScanDevicePrioritizer* host_scan_device_prioritizer,
      TetherHostResponseRecorder* tether_host_response_recorder,
      GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      NotificationPresenter* notification_presenter,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
      HostScanCache* host_scan_cache,
      ConnectionPreserver* connection_preserver,
      base::Clock* clock);
  ~HostScannerImpl() override;

  // HostScanner:
  bool IsScanActive() override;
  void StartScan() override;
  void StopScan() override;

 protected:
  // HostScannerOperation::Observer:
  void OnTetherAvailabilityResponse(
      const std::vector<HostScannerOperation::ScannedDeviceInfo>&
          scanned_device_list_so_far,
      const multidevice::RemoteDeviceRefList&
          gms_core_notifications_disabled_devices,
      bool is_final_scan_result) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  friend class HostScannerImplTest;
  FRIEND_TEST_ALL_PREFIXES(HostScannerImplTest, TestScan_ResultsFromNoDevices);

  enum HostScanResultEventType {
    NO_HOSTS_FOUND = 0,
    NOTIFICATION_SHOWN_SINGLE_HOST = 1,
    NOTIFICATION_SHOWN_MULTIPLE_HOSTS = 2,
    HOSTS_FOUND_BUT_NO_NOTIFICATION_SHOWN = 3,
    HOST_SCAN_RESULT_MAX
  };

  void OnTetherHostsFetched(
      const multidevice::RemoteDeviceRefList& tether_hosts);
  void SetCacheEntry(
      const HostScannerOperation::ScannedDeviceInfo& scanned_device_info);
  void OnFinalScanResultReceived(
      const std::vector<HostScannerOperation::ScannedDeviceInfo>&
          final_scan_results);
  void RecordHostScanResult(HostScanResultEventType event_type);
  bool IsPotentialHotspotNotificationShowing();
  bool CanAvailableHostNotificationBeShown();

  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;
  NetworkStateHandler* network_state_handler_;
  session_manager::SessionManager* session_manager_;
  TetherHostFetcher* tether_host_fetcher_;
  HostScanDevicePrioritizer* host_scan_device_prioritizer_;
  TetherHostResponseRecorder* tether_host_response_recorder_;
  GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker_;
  NotificationPresenter* notification_presenter_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;
  HostScanCache* host_scan_cache_;
  ConnectionPreserver* connection_preserver_;
  base::Clock* clock_;

  bool is_fetching_hosts_ = false;
  bool was_notification_showing_when_current_scan_started_ = false;
  bool was_notification_shown_in_current_scan_ = false;
  bool has_notification_been_shown_in_previous_scan_ = false;
  std::unique_ptr<HostScannerOperation> host_scanner_operation_;
  std::unordered_set<std::string> tether_guids_in_cache_before_scan_;

  base::ObserverList<Observer>::Unchecked observer_list_;
  base::WeakPtrFactory<HostScannerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostScannerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_IMPL_H_
