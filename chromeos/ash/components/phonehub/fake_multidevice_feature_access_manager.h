// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_

#include <vector>

#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::mojom::Feature;

}  // namespace

class FakeMultideviceFeatureAccessManager
    : public MultideviceFeatureAccessManager {
 public:
  explicit FakeMultideviceFeatureAccessManager(
      AccessStatus notification_access_status =
          AccessStatus::kAvailableButNotGranted,
      AccessStatus camera_roll_access_status =
          AccessStatus::kAvailableButNotGranted,
      AccessStatus apps_access_status = AccessStatus::kAvailableButNotGranted,
      AccessProhibitedReason reason = AccessProhibitedReason::kWorkProfile);
  ~FakeMultideviceFeatureAccessManager() override;

  using MultideviceFeatureAccessManager::IsCombinedSetupOperationInProgress;
  using MultideviceFeatureAccessManager::
      IsFeatureSetupConnectionOperationInProgress;
  using MultideviceFeatureAccessManager::IsNotificationSetupOperationInProgress;

  void SetNotificationAccessStatusInternal(
      AccessStatus notification_access_status,
      AccessProhibitedReason reason) override;
  AccessStatus GetNotificationAccessStatus() const override;
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);
  AccessProhibitedReason GetNotificationAccessProhibitedReason() const override;

  bool HasMultideviceFeatureSetupUiBeenDismissed() const override;
  void DismissSetupRequiredUi() override;
  void ResetHasMultideviceFeatureSetupUiBeenDismissed();

  void SetCameraRollAccessStatusInternal(
      AccessStatus camera_roll_access_status) override;
  AccessStatus GetCameraRollAccessStatus() const override;
  void SetCombinedSetupOperationStatus(
      CombinedAccessSetupOperation::Status new_status);

  AccessStatus GetAppsAccessStatus() const override;
  bool IsAccessRequestAllowed(Feature feature) override;

  // Test-only.
  void SetAppsAccessStatusInternal(AccessStatus apps_access_status);
  void SetFeatureReadyForAccess(Feature feature);

  void SetFeatureSetupRequestSupportedInternal(bool supported) override;
  bool GetFeatureSetupRequestSupported() const override;

  void SetFeatureSetupConnectionOperationStatus(
      FeatureSetupConnectionOperation::Status new_status);

 private:
  friend class MultideviceSetupStateUpdaterTest;
  AccessStatus notification_access_status_;
  AccessStatus camera_roll_access_status_;
  AccessStatus apps_access_status_;
  AccessProhibitedReason access_prohibited_reason_;
  bool has_notification_setup_ui_been_dismissed_ = false;
  std::vector<Feature> ready_for_access_features_;
  bool is_feature_setup_request_supported_ = false;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
