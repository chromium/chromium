// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker.h"
#include "chromeos/ash/components/tether/host_scanner_operation.h"

namespace ash {

namespace tether {

// Concrete GmsCoreNotificationsStateTracker implementation.
class GmsCoreNotificationsStateTrackerImpl
    : public GmsCoreNotificationsStateTracker,
      public HostScannerOperation::Observer {
 public:
  GmsCoreNotificationsStateTrackerImpl();

  GmsCoreNotificationsStateTrackerImpl(
      const GmsCoreNotificationsStateTrackerImpl&) = delete;
  GmsCoreNotificationsStateTrackerImpl& operator=(
      const GmsCoreNotificationsStateTrackerImpl&) = delete;

  ~GmsCoreNotificationsStateTrackerImpl() override;

  // GmsCoreNotificationsStateTracker:
  std::vector<std::string> GetGmsCoreNotificationsDisabledDeviceNames()
      override;

 protected:
  // HostScannerOperation::Observer:
  void OnTetherAvailabilityResponse(
      const std::vector<HostScannerOperation::ScannedDeviceInfo>&
          scanned_device_list_so_far,
      const multidevice::RemoteDeviceRefList&
          gms_core_notifications_disabled_devices,
      bool is_final_scan_result) override;

 private:
  friend class GmsCoreNotificationsStateTrackerImplTest;

  void SendDeviceNamesChangeEvent();

  std::map<std::string, std::string> device_id_to_name_map_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_IMPL_H_
