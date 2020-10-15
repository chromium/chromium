// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_

#include "chromeos/components/phonehub/notification_access_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace phonehub {

// This class enables the PhoneHub Multidevice feature state when the HostStatus
// (provided by the MultideviceSetupClient) of the phone is initially
// |kHostSetLocallyButWaitingForBackendConfirmation|, and becomes either 1)
// |kHostVerified|, or 2) becomes |kHostSetButNotYetVerified| first, and then
// later |kHostVerified|. This class also disables the PhoneHubNotification
// Multidevice feature state when Notification access has been revoked by the
// phone, provided via NotificationAccessManager.
class MultideviceSetupStateUpdater
    : public multidevice_setup::MultiDeviceSetupClient::Observer,
      public NotificationAccessManager::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  MultideviceSetupStateUpdater(
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      NotificationAccessManager* notification_access_manager);
  ~MultideviceSetupStateUpdater() override;

 private:
  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  // NotificationAccessManager::Observer:
  void OnNotificationAccessChanged() override;

  void UpdateIsAwaitingVerifiedHost(
      multidevice_setup::mojom::HostStatus host_status);

  PrefService* pref_service_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  NotificationAccessManager* notification_access_manager_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_
