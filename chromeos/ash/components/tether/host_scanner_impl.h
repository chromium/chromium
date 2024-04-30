// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCANNER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCANNER_IMPL_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/host_scanner.h"
#include "chromeos/ash/components/tether/notification_presenter.h"
#include "chromeos/ash/components/tether/tether_availability_operation_orchestrator.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace session_manager {
class SessionManager;
}

namespace ash::tether {

class DeviceIdTetherNetworkGuidMap;
class GmsCoreNotificationsStateTrackerImpl;
class HostScanCache;

// Scans for nearby tether hosts. When StartScan() is called, this class creates
// a new TetherAvailabilityOperation and uses it to contact nearby devices to
// query whether they can provide tether capabilities. Once the scan results are
// received, they are stored in the HostScanCache passed to the constructor,
// and observers are notified via HostScanner::Observer::ScanFinished().
class HostScannerImpl
    : public HostScanner,
      public TetherAvailabilityOperationOrchestrator::Observer,
      public session_manager::SessionManagerObserver {
 public:
  class Observer {
   public:
    void virtual ScanFinished() = 0;
  };

  HostScannerImpl(
      std::unique_ptr<TetherAvailabilityOperationOrchestrator::Factory>
          tether_availability_operation_orchestrator_factory,
      NetworkStateHandler* network_state_handler,
      session_manager::SessionManager* session_manager,
      GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      NotificationPresenter* notification_presenter,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
      HostScanCache* host_scan_cache,
      base::Clock* clock);

  HostScannerImpl(const HostScannerImpl&) = delete;
  HostScannerImpl& operator=(const HostScannerImpl&) = delete;

  ~HostScannerImpl() override;

  // HostScanner:
  bool IsScanActive() override;
  void StartScan() override;
  void StopScan() override;

 protected:
  // TetherAvailabilityOperationOrchestrator::Observer:
  void OnTetherAvailabilityResponse(
      const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
      const std::vector<ScannedDeviceInfo>&
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

  void SetCacheEntry(const ScannedDeviceInfo& scanned_device_info);
  void OnFinalScanResultReceived(
      const std::vector<ScannedDeviceInfo>& final_scan_results);
  void RecordHostScanResult(HostScanResultEventType event_type);
  bool IsPotentialHotspotNotificationShowing();
  bool CanAvailableHostNotificationBeShown();

  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<session_manager::SessionManager> session_manager_;
  raw_ptr<GmsCoreNotificationsStateTrackerImpl>
      gms_core_notifications_state_tracker_;
  raw_ptr<NotificationPresenter> notification_presenter_;
  raw_ptr<DeviceIdTetherNetworkGuidMap> device_id_tether_network_guid_map_;
  raw_ptr<HostScanCache> host_scan_cache_;
  raw_ptr<base::Clock> clock_;

  bool was_notification_showing_when_current_scan_started_ = false;
  bool was_notification_shown_in_current_scan_ = false;
  bool has_notification_been_shown_in_previous_scan_ = false;
  std::unique_ptr<TetherAvailabilityOperationOrchestrator::Factory>
      tether_availability_operation_orchestrator_factory_;
  std::unique_ptr<TetherAvailabilityOperationOrchestrator>
      tether_availability_operation_orchestrator_;
  std::unordered_set<std::string> tether_guids_in_cache_before_scan_;

  base::ObserverList<Observer>::Unchecked observer_list_;
  base::WeakPtrFactory<HostScannerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCANNER_IMPL_H_
