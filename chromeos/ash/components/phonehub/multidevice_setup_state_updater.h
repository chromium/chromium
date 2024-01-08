// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

// This class waits until a multi-device host phone is verified before enabling
// the Phone Hub feature. This intent to enable the feature is persisted across
// restarts. This class also disables the PhoneHubNotification Multidevice
// feature state when Notification access has been revoked by the phone,
// provided via MultideviceFeatureAccessManager.
class MultideviceSetupStateUpdater
    : public multidevice_setup::MultiDeviceSetupClient::Observer,
      public MultideviceFeatureAccessManager::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  MultideviceSetupStateUpdater(
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      MultideviceFeatureAccessManager* multidevice_feature_access_manager);
  ~MultideviceSetupStateUpdater() override;

 private:
  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_state_map) override;

  // MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override;
  void OnCameraRollAccessChanged() override;

  bool IsWaitingForAccessToInitiallyEnableNotifications() const;
  bool IsWaitingForAccessToInitiallyEnableCameraRoll() const;
  bool IsPhoneHubEnabled() const;
  void EnablePhoneHubIfAwaitingVerifiedHost();
  void UpdateIsAwaitingVerifiedHost();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<MultideviceFeatureAccessManager> multidevice_feature_access_manager_;
  MultideviceFeatureAccessManager::AccessStatus notification_access_status_;
  MultideviceFeatureAccessManager::AccessStatus camera_roll_access_status_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_
