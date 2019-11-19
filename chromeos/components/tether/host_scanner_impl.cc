// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/connection_preserver.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/device_status_util.h"
#include "chromeos/components/tether/gms_core_notifications_state_tracker_impl.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/components/tether/master_host_scan_cache.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/network/network_state.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

namespace tether {

HostScannerImpl::HostScannerImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    NetworkStateHandler* network_state_handler,
    session_manager::SessionManager* session_manager,
    TetherHostFetcher* tether_host_fetcher,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder,
    GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker,
    NotificationPresenter* notification_presenter,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    HostScanCache* host_scan_cache,
    ConnectionPreserver* connection_preserver,
    base::Clock* clock)
    : device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      network_state_handler_(network_state_handler),
      session_manager_(session_manager),
      tether_host_fetcher_(tether_host_fetcher),
      host_scan_device_prioritizer_(host_scan_device_prioritizer),
      tether_host_response_recorder_(tether_host_response_recorder),
      gms_core_notifications_state_tracker_(
          gms_core_notifications_state_tracker),
      notification_presenter_(notification_presenter),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      host_scan_cache_(host_scan_cache),
      connection_preserver_(connection_preserver),
      clock_(clock) {
  session_manager_->AddObserver(this);
}

HostScannerImpl::~HostScannerImpl() {
  session_manager_->RemoveObserver(this);
}

bool HostScannerImpl::IsScanActive() {
  return is_fetching_hosts_ || host_scanner_operation_;
}

void HostScannerImpl::StartScan() {
  if (IsScanActive())
    return;

  is_fetching_hosts_ = true;
  tether_host_fetcher_->FetchAllTetherHosts(base::Bind(
      &HostScannerImpl::OnTetherHostsFetched, weak_ptr_factory_.GetWeakPtr()));
}

void HostScannerImpl::StopScan() {
  if (!host_scanner_operation_)
    return;

  PA_LOG(VERBOSE) << "Host scan has been stopped prematurely.";

  host_scanner_operation_->RemoveObserver(
      gms_core_notifications_state_tracker_);
  host_scanner_operation_->RemoveObserver(this);
  host_scanner_operation_.reset();

  NotifyScanFinished();
}

void HostScannerImpl::OnTetherHostsFetched(
    const multidevice::RemoteDeviceRefList& tether_hosts) {
  is_fetching_hosts_ = false;

  if (tether_hosts.empty()) {
    PA_LOG(WARNING) << "Could not start host scan. No tether hosts available.";
    return;
  }

  PA_LOG(VERBOSE) << "Starting Tether host scan. " << tether_hosts.size() << " "
                  << "potential host(s) included in the search.";

  tether_guids_in_cache_before_scan_ =
      host_scan_cache_->GetTetherGuidsInCache();

  host_scanner_operation_ = HostScannerOperation::Factory::NewInstance(
      tether_hosts, device_sync_client_, secure_channel_client_,
      host_scan_device_prioritizer_, tether_host_response_recorder_,
      connection_preserver_);
  // Add |gms_core_notifications_state_tracker_| as the first observer. When the
  // final change event is emitted, this class will destroy
  // |host_scanner_operation_|, so |gms_core_notifications_state_tracker_| must
  // be notified of the final change event before that occurs.
  host_scanner_operation_->AddObserver(gms_core_notifications_state_tracker_);
  host_scanner_operation_->AddObserver(this);
  host_scanner_operation_->Initialize();
}

void HostScannerImpl::OnTetherAvailabilityResponse(
    const std::vector<HostScannerOperation::ScannedDeviceInfo>&
        scanned_device_list_so_far,
    const multidevice::RemoteDeviceRefList&
        gms_core_notifications_disabled_devices,
    bool is_final_scan_result) {
  if (scanned_device_list_so_far.empty() && !is_final_scan_result) {
    was_notification_showing_when_current_scan_started_ =
        IsPotentialHotspotNotificationShowing();
  }

  // Ensure all results received so far are in the cache (setting entries which
  // already exist is a no-op).
  for (const auto& scanned_device_info : scanned_device_list_so_far)
    SetCacheEntry(scanned_device_info);

  if (CanAvailableHostNotificationBeShown() &&
      !scanned_device_list_so_far.empty()) {
    if (scanned_device_list_so_far.size() == 1u &&
        (notification_presenter_->GetPotentialHotspotNotificationState() !=
             NotificationPresenter::PotentialHotspotNotificationState::
                 MULTIPLE_HOTSPOTS_NEARBY_SHOWN ||
         is_final_scan_result)) {
      multidevice::RemoteDeviceRef remote_device =
          scanned_device_list_so_far.at(0).remote_device;
      int32_t signal_strength;
      NormalizeDeviceStatus(scanned_device_list_so_far.at(0).device_status,
                            nullptr /* carrier */,
                            nullptr /* battery_percentage */, &signal_strength);
      notification_presenter_->NotifyPotentialHotspotNearby(remote_device,
                                                            signal_strength);
    } else {
      // Note: If a single-device notification was previously displayed, calling
      // NotifyMultiplePotentialHotspotsNearby() will reuse the existing
      // notification.
      notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
    }

    was_notification_shown_in_current_scan_ = true;
  }

  if (is_final_scan_result)
    OnFinalScanResultReceived(scanned_device_list_so_far);
}

void HostScannerImpl::OnSessionStateChanged() {
  if (!has_notification_been_shown_in_previous_scan_ ||
      !session_manager_->IsScreenLocked()) {
    return;
  }

  // If the screen has been locked, reset
  // |has_notification_been_shown_in_previous_scan_|. This allows the
  // notification to be shown once each time the device is unlocked. Without
  // this change, the notification would only be shown once per user login.
  // See https://crbug.com/813838.
  PA_LOG(VERBOSE)
      << "Screen was locked; the \"available hosts\" notification can "
      << "be shown again after the next unlock.";
  has_notification_been_shown_in_previous_scan_ = false;
}

void HostScannerImpl::SetCacheEntry(
    const HostScannerOperation::ScannedDeviceInfo& scanned_device_info) {
  const DeviceStatus& status = scanned_device_info.device_status;
  multidevice::RemoteDeviceRef remote_device =
      scanned_device_info.remote_device;

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;
  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  host_scan_cache_->SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(device_id_tether_network_guid_map_
                                     ->GetTetherNetworkGuidForDeviceId(
                                         remote_device.GetDeviceId()))
           .SetDeviceName(remote_device.name())
           .SetCarrier(carrier)
           .SetBatteryPercentage(battery_percentage)
           .SetSignalStrength(signal_strength)
           .SetSetupRequired(scanned_device_info.setup_required)
           .Build());
}

void HostScannerImpl::OnFinalScanResultReceived(
    const std::vector<HostScannerOperation::ScannedDeviceInfo>&
        final_scan_results) {
  // Search through all GUIDs that were in the cache before the scan began. If
  // any of those GUIDs are not present in the final scan results, remove them
  // from the cache.
  for (const auto& tether_guid_in_cache : tether_guids_in_cache_before_scan_) {
    bool is_guid_in_final_scan_results = false;

    for (const auto& scan_result : final_scan_results) {
      if (device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
              scan_result.remote_device.GetDeviceId()) ==
          tether_guid_in_cache) {
        is_guid_in_final_scan_results = true;
        break;
      }
    }

    if (!is_guid_in_final_scan_results)
      host_scan_cache_->RemoveHostScanResult(tether_guid_in_cache);
  }

  if (final_scan_results.empty()) {
    RecordHostScanResult(HostScanResultEventType::NO_HOSTS_FOUND);
  } else if (!was_notification_shown_in_current_scan_) {
    RecordHostScanResult(
        HostScanResultEventType::HOSTS_FOUND_BUT_NO_NOTIFICATION_SHOWN);
  } else if (final_scan_results.size() == 1u) {
    RecordHostScanResult(
        HostScanResultEventType::NOTIFICATION_SHOWN_SINGLE_HOST);
  } else {
    RecordHostScanResult(
        HostScanResultEventType::NOTIFICATION_SHOWN_MULTIPLE_HOSTS);
  }
  has_notification_been_shown_in_previous_scan_ |=
      was_notification_shown_in_current_scan_;
  was_notification_shown_in_current_scan_ = false;
  was_notification_showing_when_current_scan_started_ = false;

  PA_LOG(VERBOSE) << "Finished Tether host scan. " << final_scan_results.size()
                  << " potential host(s) were found.";

  // If the final scan result has been received, the operation is finished.
  // Delete it.
  host_scanner_operation_->RemoveObserver(
      gms_core_notifications_state_tracker_);
  host_scanner_operation_->RemoveObserver(this);
  host_scanner_operation_.reset();

  NotifyScanFinished();
}

void HostScannerImpl::RecordHostScanResult(HostScanResultEventType event_type) {
  DCHECK(event_type != HostScanResultEventType::HOST_SCAN_RESULT_MAX);
  UMA_HISTOGRAM_ENUMERATION("InstantTethering.HostScanResult", event_type,
                            HostScanResultEventType::HOST_SCAN_RESULT_MAX);
}

bool HostScannerImpl::IsPotentialHotspotNotificationShowing() {
  return notification_presenter_->GetPotentialHotspotNotificationState() !=
         NotificationPresenter::PotentialHotspotNotificationState::
             NO_HOTSPOT_NOTIFICATION_SHOWN;
}

bool HostScannerImpl::CanAvailableHostNotificationBeShown() {
  const chromeos::NetworkTypePattern network_type_pattern =
      chromeos::switches::ShouldTetherHostScansIgnoreWiredConnections()
          ? chromeos::NetworkTypePattern::Wireless()
          : chromeos::NetworkTypePattern::Default();
  // Note: If a network is active (i.e., connecting or connected), it will be
  // returned at the front of the list, so using FirstNetworkByType() guarantees
  // that we will find an active network if there is one.
  const chromeos::NetworkState* first_network =
      network_state_handler_->FirstNetworkByType(network_type_pattern);
  if (first_network && first_network->IsConnectingOrConnected()) {
    // If a network is connecting or connected, the notification should not be
    // shown.
    return false;
  }

  if (!IsPotentialHotspotNotificationShowing() &&
      was_notification_shown_in_current_scan_) {
    // If a notification was shown in the current scan but it is no longer
    // showing, it has been removed, either due to NotificationRemover or due to
    // the user closing it. Since a scan only lasts on the order of seconds to
    // tens of seconds, we know that the notification was very recently closed,
    // so we should not re-show it.
    return false;
  }

  if (!IsPotentialHotspotNotificationShowing() &&
      was_notification_showing_when_current_scan_started_) {
    // If a notification was showing when the scan started but is no longer
    // showing, it has been removed and should not be re-shown.
    return false;
  }

  if (has_notification_been_shown_in_previous_scan_ &&
      !was_notification_showing_when_current_scan_started_) {
    // If a notification was shown in a previous scan but was not visible when
    // the current scan started, it should not be shown because this could be
    // considered spammy; see https://crbug.com/759078.
    return false;
  }

  return true;
}

}  // namespace tether

}  // namespace chromeos
