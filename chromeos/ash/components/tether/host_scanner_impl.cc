// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scanner_impl.h"

#include <algorithm>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/tether/connection_preserver.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/device_status_util.h"
#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker_impl.h"
#include "chromeos/ash/components/tether/host_scan_cache.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/components/tether/top_level_host_scan_cache.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/session_manager/core/session_manager.h"

namespace ash::tether {

HostScannerImpl::HostScannerImpl(
    std::unique_ptr<TetherAvailabilityOperationOrchestrator::Factory>
        tether_availability_operation_orchestrator_factory,
    NetworkStateHandler* network_state_handler,
    session_manager::SessionManager* session_manager,
    GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker,
    NotificationPresenter* notification_presenter,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    HostScanCache* host_scan_cache,
    base::Clock* clock)
    : network_state_handler_(network_state_handler),
      session_manager_(session_manager),
      gms_core_notifications_state_tracker_(
          gms_core_notifications_state_tracker),
      notification_presenter_(notification_presenter),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      host_scan_cache_(host_scan_cache),
      clock_(clock),
      tether_availability_operation_orchestrator_factory_(
          std::move(tether_availability_operation_orchestrator_factory)) {
  session_manager_->AddObserver(this);
}

HostScannerImpl::~HostScannerImpl() {
  session_manager_->RemoveObserver(this);
}

bool HostScannerImpl::IsScanActive() {
  return tether_availability_operation_orchestrator_ != nullptr;
}

void HostScannerImpl::StartScan() {
  if (IsScanActive()) {
    PA_LOG(ERROR)
        << "Not starting host scan, as a tether host scan is already active.";
    return;
  }

  PA_LOG(VERBOSE) << "Starting tether host scan.";

  tether_guids_in_cache_before_scan_ =
      host_scan_cache_->GetTetherGuidsInCache();

  PA_LOG(VERBOSE) << "Constructing TetherAvailabilityOperationOrchestrator";
  tether_availability_operation_orchestrator_ =
      tether_availability_operation_orchestrator_factory_->CreateInstance();

  tether_availability_operation_orchestrator_->AddObserver(
      gms_core_notifications_state_tracker_);
  tether_availability_operation_orchestrator_->AddObserver(this);

  PA_LOG(VERBOSE) << "Starting TetherAvailabilityOperationOrchestrator";
  tether_availability_operation_orchestrator_->Start();
}

void HostScannerImpl::StopScan() {
  if (!tether_availability_operation_orchestrator_) {
    return;
  }

  PA_LOG(VERBOSE) << "Host scan has been stopped prematurely.";

  tether_availability_operation_orchestrator_->RemoveObserver(
      gms_core_notifications_state_tracker_);
  tether_availability_operation_orchestrator_->RemoveObserver(this);
  tether_availability_operation_orchestrator_.reset();

  NotifyScanFinished();
}

void HostScannerImpl::OnTetherAvailabilityResponse(
    const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
    const std::vector<ScannedDeviceInfo>&
        gms_core_notifications_disabled_devices,
    bool is_final_scan_result) {
  if (scanned_device_list_so_far.empty() && !is_final_scan_result) {
    was_notification_showing_when_current_scan_started_ =
        IsPotentialHotspotNotificationShowing();
    PA_LOG(VERBOSE) << "Was 'potential hotspot' notification showing when "
                       "current scan started: ["
                    << was_notification_showing_when_current_scan_started_
                    << "]";
  }

  // Ensure all results received so far are in the cache (setting entries which
  // already exist is a no-op).
  for (const auto& scanned_device_info : scanned_device_list_so_far) {
    SetCacheEntry(scanned_device_info);
  }

  if (CanAvailableHostNotificationBeShown() &&
      !scanned_device_list_so_far.empty()) {
    if (scanned_device_list_so_far.size() == 1u &&
        (notification_presenter_->GetPotentialHotspotNotificationState() !=
             NotificationPresenter::PotentialHotspotNotificationState::
                 MULTIPLE_HOTSPOTS_NEARBY_SHOWN ||
         is_final_scan_result)) {
      const ScannedDeviceInfo& scanned_device =
          scanned_device_list_so_far.front();
      int32_t signal_strength;
      NormalizeDeviceStatus(scanned_device.device_status.value(),
                            nullptr /* carrier */,
                            nullptr /* battery_percentage */, &signal_strength);
      notification_presenter_->NotifyPotentialHotspotNearby(
          scanned_device.device_id, scanned_device.device_name,
          signal_strength);
    } else {
      // Note: If a single-device notification was previously displayed, calling
      // NotifyMultiplePotentialHotspotsNearby() will reuse the existing
      // notification.
      notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
    }

    was_notification_shown_in_current_scan_ = true;
  }

  if (is_final_scan_result) {
    OnFinalScanResultReceived(scanned_device_list_so_far);
  }
}

void HostScannerImpl::OnSessionStateChanged() {
  TRACE_EVENT0("login", "HostScannerImpl::OnSessionStateChanged");
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
    const ScannedDeviceInfo& scanned_device_info) {
  const DeviceStatus& status = scanned_device_info.device_status.value();

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;
  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  host_scan_cache_->SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(device_id_tether_network_guid_map_
                                     ->GetTetherNetworkGuidForDeviceId(
                                         scanned_device_info.device_id))
           .SetDeviceName(scanned_device_info.device_name)
           .SetCarrier(carrier)
           .SetBatteryPercentage(battery_percentage)
           .SetSignalStrength(signal_strength)
           .SetSetupRequired(scanned_device_info.setup_required)
           .Build());
}

void HostScannerImpl::OnFinalScanResultReceived(
    const std::vector<ScannedDeviceInfo>& final_scan_results) {
  PA_LOG(INFO) << __func__;
  // Search through all GUIDs that were in the cache before the scan began. If
  // any of those GUIDs are not present in the final scan results, remove them
  // from the cache.
  for (const auto& tether_guid_in_cache : tether_guids_in_cache_before_scan_) {
    bool is_guid_in_final_scan_results = false;

    for (const auto& scan_result : final_scan_results) {
      if (device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
              scan_result.device_id) == tether_guid_in_cache) {
        is_guid_in_final_scan_results = true;
        break;
      }
    }

    if (!is_guid_in_final_scan_results) {
      host_scan_cache_->RemoveHostScanResult(tether_guid_in_cache);
    }
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
  tether_availability_operation_orchestrator_->RemoveObserver(
      gms_core_notifications_state_tracker_);
  tether_availability_operation_orchestrator_->RemoveObserver(this);
  tether_availability_operation_orchestrator_.reset();

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
  const NetworkTypePattern network_type_pattern =
      switches::ShouldTetherHostScansIgnoreWiredConnections()
          ? NetworkTypePattern::Wireless()
          : NetworkTypePattern::Default();
  // Note: If a network is active (i.e., connecting or connected), it will be
  // returned at the front of the list, so using FirstNetworkByType() guarantees
  // that we will find an active network if there is one.
  const NetworkState* first_network =
      network_state_handler_->FirstNetworkByType(network_type_pattern);
  if (first_network && first_network->IsConnectingOrConnected()) {
    // If a network is connecting or connected, the notification should not be
    // shown.
    PA_LOG(INFO)
        << __func__
        << " Returning false because device is connected/connecting to "
           "a network";
    return false;
  }

  if (!IsPotentialHotspotNotificationShowing() &&
      was_notification_shown_in_current_scan_) {
    PA_LOG(INFO) << __func__
                 << " Returning false because notification has been shown in "
                    "the current scan";
    // If a notification was shown in the current scan but it is no longer
    // showing, it has been removed, either due to NotificationRemover or due to
    // the user closing it. Since a scan only lasts on the order of seconds to
    // tens of seconds, we know that the notification was very recently closed,
    // so we should not re-show it.
    return false;
  }

  if (!IsPotentialHotspotNotificationShowing() &&
      was_notification_showing_when_current_scan_started_) {
    PA_LOG(INFO) << __func__
                 << " Returning false because notification was showing when "
                    "scan started";
    // If a notification was showing when the scan started but is no longer
    // showing, it has been removed and should not be re-shown.
    return false;
  }

  PA_LOG(INFO) << __func__ << " Returning true because all checks passed";

  return true;
}

}  // namespace ash::tether
