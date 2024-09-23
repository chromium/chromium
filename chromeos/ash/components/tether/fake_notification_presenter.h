// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_NOTIFICATION_PRESENTER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_NOTIFICATION_PRESENTER_H_

#include <memory>
#include <optional>
#include <string>

#include "chromeos/ash/components/tether/notification_presenter.h"

namespace ash::tether {

class FakeNotificationPresenter : public NotificationPresenter {
 public:
  FakeNotificationPresenter();

  FakeNotificationPresenter(const FakeNotificationPresenter&) = delete;
  FakeNotificationPresenter& operator=(const FakeNotificationPresenter&) =
      delete;

  ~FakeNotificationPresenter() override;

  // Note: This function fails a test if potential_hotspot_state() is not
  // SINGLE_HOTSPOT_NEARBY_SHOWN when called.
  std::optional<std::string> GetPotentialHotspotRemoteDeviceId();

  bool is_setup_required_notification_shown() {
    return is_setup_required_notification_shown_;
  }

  bool is_connection_failed_notification_shown() {
    return is_connection_failed_notification_shown_;
  }

  // NotificationPresenter:
  void NotifyPotentialHotspotNearby(const std::string& device_id,
                                    const std::string& device_name,
                                    int signal_strength) override;
  void NotifyMultiplePotentialHotspotsNearby() override;
  PotentialHotspotNotificationState GetPotentialHotspotNotificationState()
      override;
  void RemovePotentialHotspotNotification() override;
  void NotifySetupRequired(const std::string& device_name,
                           int signal_strength) override;
  void RemoveSetupRequiredNotification() override;
  void NotifyConnectionToHostFailed() override;
  void RemoveConnectionToHostFailedNotification() override;

 private:
  PotentialHotspotNotificationState potential_hotspot_state_;
  std::optional<std::string> potential_hotspot_tether_host_id_;
  bool is_setup_required_notification_shown_;
  bool is_connection_failed_notification_shown_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_NOTIFICATION_PRESENTER_H_
