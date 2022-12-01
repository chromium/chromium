// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"

namespace ash {
namespace phonehub {

FakeMultideviceFeatureAccessManager::FakeMultideviceFeatureAccessManager(
    AccessStatus notification_access_status,
    AccessStatus camera_roll_access_status,
    AccessStatus apps_access_status,
    AccessProhibitedReason reason)
    : notification_access_status_(notification_access_status),
      camera_roll_access_status_(camera_roll_access_status),
      apps_access_status_(apps_access_status),
      access_prohibited_reason_(reason) {
  ready_for_access_features_ = {};
}

FakeMultideviceFeatureAccessManager::~FakeMultideviceFeatureAccessManager() =
    default;

void FakeMultideviceFeatureAccessManager::SetNotificationAccessStatusInternal(
    AccessStatus notification_access_status,
    AccessProhibitedReason reason) {
  if (notification_access_status_ == notification_access_status)
    return;

  notification_access_status_ = notification_access_status;
  access_prohibited_reason_ = reason;
  NotifyNotificationAccessChanged();
}

MultideviceFeatureAccessManager::AccessProhibitedReason
FakeMultideviceFeatureAccessManager::GetNotificationAccessProhibitedReason()
    const {
  return access_prohibited_reason_;
}

void FakeMultideviceFeatureAccessManager::SetCameraRollAccessStatusInternal(
    AccessStatus camera_roll_access_status) {
  if (camera_roll_access_status_ == camera_roll_access_status)
    return;

  camera_roll_access_status_ = camera_roll_access_status;
  NotifyCameraRollAccessChanged();
}

void FakeMultideviceFeatureAccessManager::SetAppsAccessStatusInternal(
    AccessStatus apps_access_status) {
  if (apps_access_status_ == apps_access_status)
    return;

  apps_access_status_ = apps_access_status;
  NotifyAppsAccessChanged();
}

void FakeMultideviceFeatureAccessManager::SetFeatureReadyForAccess(
    multidevice_setup::mojom::Feature feature) {
  ready_for_access_features_.push_back(feature);
}

bool FakeMultideviceFeatureAccessManager::IsAccessRequestAllowed(
    multidevice_setup::mojom::Feature feature) {
  return base::Contains(ready_for_access_features_, feature);
}

MultideviceFeatureAccessManager::AccessStatus
FakeMultideviceFeatureAccessManager::GetAppsAccessStatus() const {
  return apps_access_status_;
}

MultideviceFeatureAccessManager::AccessStatus
FakeMultideviceFeatureAccessManager::GetNotificationAccessStatus() const {
  return notification_access_status_;
}
MultideviceFeatureAccessManager::AccessStatus
FakeMultideviceFeatureAccessManager::GetCameraRollAccessStatus() const {
  return camera_roll_access_status_;
}

bool FakeMultideviceFeatureAccessManager::
    HasMultideviceFeatureSetupUiBeenDismissed() const {
  return has_notification_setup_ui_been_dismissed_;
}

void FakeMultideviceFeatureAccessManager::DismissSetupRequiredUi() {
  has_notification_setup_ui_been_dismissed_ = true;
}

void FakeMultideviceFeatureAccessManager::
    ResetHasMultideviceFeatureSetupUiBeenDismissed() {
  has_notification_setup_ui_been_dismissed_ = false;
}

void FakeMultideviceFeatureAccessManager::SetNotificationSetupOperationStatus(
    NotificationAccessSetupOperation::Status new_status) {
  switch (new_status) {
    case NotificationAccessSetupOperation::Status::kCompletedSuccessfully:
      SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                          AccessProhibitedReason::kUnknown);
      break;
    case NotificationAccessSetupOperation::Status::
        kProhibitedFromProvidingAccess:
      SetNotificationAccessStatusInternal(
          AccessStatus::kProhibited,
          AccessProhibitedReason::kDisabledByPhonePolicy);
      break;
    default:
      // Do not update access status based on other operation status values.
      break;
  }

  MultideviceFeatureAccessManager::SetNotificationSetupOperationStatus(
      new_status);
}

void FakeMultideviceFeatureAccessManager::SetCombinedSetupOperationStatus(
    CombinedAccessSetupOperation::Status new_status) {
  if (new_status ==
      CombinedAccessSetupOperation::Status::kCompletedSuccessfully) {
    SetCameraRollAccessStatusInternal(AccessStatus::kAccessGranted);
  }
  MultideviceFeatureAccessManager::SetCombinedSetupOperationStatus(new_status);
}

void FakeMultideviceFeatureAccessManager::
    SetFeatureSetupRequestSupportedInternal(bool supported) {
  is_feature_setup_request_supported_ = supported;
}

bool FakeMultideviceFeatureAccessManager::GetFeatureSetupRequestSupported()
    const {
  return is_feature_setup_request_supported_;
}

void FakeMultideviceFeatureAccessManager::
    SetFeatureSetupConnectionOperationStatus(
        FeatureSetupConnectionOperation::Status new_status) {
  MultideviceFeatureAccessManager::SetFeatureSetupConnectionOperationStatus(
      new_status);
}

}  // namespace phonehub
}  // namespace ash
