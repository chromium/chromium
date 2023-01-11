// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_notification_presenter.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

FakeNotificationPresenter::FakeNotificationPresenter()
    : NotificationPresenter(),
      potential_hotspot_state_(
          NotificationPresenter::PotentialHotspotNotificationState::
              NO_HOTSPOT_NOTIFICATION_SHOWN),
      is_setup_required_notification_shown_(false),
      is_connection_failed_notification_shown_(false) {}

FakeNotificationPresenter::~FakeNotificationPresenter() = default;

absl::optional<multidevice::RemoteDeviceRef>
FakeNotificationPresenter::GetPotentialHotspotRemoteDevice() {
  EXPECT_EQ(potential_hotspot_state_,
            NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN);
  return potential_hotspot_remote_device_;
}

void FakeNotificationPresenter::NotifyPotentialHotspotNearby(
    multidevice::RemoteDeviceRef remote_device,
    int signal_strength) {
  potential_hotspot_state_ = NotificationPresenter::
      PotentialHotspotNotificationState::SINGLE_HOTSPOT_NEARBY_SHOWN;
  potential_hotspot_remote_device_ = remote_device;
}

void FakeNotificationPresenter::NotifyMultiplePotentialHotspotsNearby() {
  potential_hotspot_state_ = NotificationPresenter::
      PotentialHotspotNotificationState::MULTIPLE_HOTSPOTS_NEARBY_SHOWN;
}

NotificationPresenter::PotentialHotspotNotificationState
FakeNotificationPresenter::GetPotentialHotspotNotificationState() {
  return potential_hotspot_state_;
}

void FakeNotificationPresenter::RemovePotentialHotspotNotification() {
  potential_hotspot_state_ = NotificationPresenter::
      PotentialHotspotNotificationState::NO_HOTSPOT_NOTIFICATION_SHOWN;
}

void FakeNotificationPresenter::NotifySetupRequired(
    const std::string& device_name,
    int signal_strength) {
  is_setup_required_notification_shown_ = true;
}

void FakeNotificationPresenter::RemoveSetupRequiredNotification() {
  is_setup_required_notification_shown_ = false;
}

void FakeNotificationPresenter::NotifyConnectionToHostFailed() {
  is_connection_failed_notification_shown_ = true;
}

void FakeNotificationPresenter::RemoveConnectionToHostFailedNotification() {
  is_connection_failed_notification_shown_ = false;
}

}  // namespace tether

}  // namespace ash
