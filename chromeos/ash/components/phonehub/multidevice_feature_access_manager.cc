// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"

namespace ash {
namespace phonehub {

MultideviceFeatureAccessManager::MultideviceFeatureAccessManager() = default;

MultideviceFeatureAccessManager::~MultideviceFeatureAccessManager() = default;

std::unique_ptr<NotificationAccessSetupOperation>
MultideviceFeatureAccessManager::AttemptNotificationSetup(
    NotificationAccessSetupOperation::Delegate* delegate) {
  // Should only be able to start the setup process if notification access is
  // available but not yet granted.
  // Legacy setup flow used when FeatureSetupRequest is not supported.
  if (GetNotificationAccessStatus() != AccessStatus::kAvailableButNotGranted)
    return nullptr;

  int operation_id = next_operation_id_;
  ++next_operation_id_;

  auto operation = base::WrapUnique(new NotificationAccessSetupOperation(
      delegate,
      base::BindOnce(
          &MultideviceFeatureAccessManager::OnNotificationSetupOperationDeleted,
          weak_ptr_factory_.GetWeakPtr(), operation_id)));
  id_to_notification_operation_map_.emplace(operation_id, operation.get());

  OnNotificationSetupRequested();
  return operation;
}

std::unique_ptr<CombinedAccessSetupOperation>
MultideviceFeatureAccessManager::AttemptCombinedFeatureSetup(
    bool camera_roll,
    bool notifications,
    CombinedAccessSetupOperation::Delegate* delegate) {
  // New setup flow for combined Camera Roll and/or Notifications setup using
  // FeatureSetupRequest message type.
  if (!GetFeatureSetupRequestSupported()) {
    return nullptr;
  }
  if (GetCameraRollAccessStatus() != AccessStatus::kAvailableButNotGranted &&
      camera_roll) {
    return nullptr;
  }
  if (GetNotificationAccessStatus() != AccessStatus::kAvailableButNotGranted &&
      notifications) {
    return nullptr;
  }

  int operation_id = next_operation_id_;
  ++next_operation_id_;

  auto operation = base::WrapUnique(new CombinedAccessSetupOperation(
      delegate,
      base::BindOnce(
          &MultideviceFeatureAccessManager::OnCombinedSetupOperationDeleted,
          weak_ptr_factory_.GetWeakPtr(), operation_id)));
  id_to_combined_operation_map_.emplace(operation_id, operation.get());

  OnCombinedSetupRequested(camera_roll, notifications);
  return operation;
}

std::unique_ptr<FeatureSetupConnectionOperation>
MultideviceFeatureAccessManager::AttemptFeatureSetupConnection(
    FeatureSetupConnectionOperation::Delegate* delegate) {
  int operation_id = next_operation_id_;
  ++next_operation_id_;

  auto operation = base::WrapUnique(new FeatureSetupConnectionOperation(
      delegate, base::BindOnce(&MultideviceFeatureAccessManager::
                                   OnFeatureSetupConnectionOperationDeleted,
                               weak_ptr_factory_.GetWeakPtr(), operation_id)));
  id_to_connection_operation_map_.emplace(operation_id, operation.get());

  OnFeatureSetupConnectionRequested();
  return operation;
}

void MultideviceFeatureAccessManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MultideviceFeatureAccessManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MultideviceFeatureAccessManager::NotifyNotificationAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnNotificationAccessChanged();
}

void MultideviceFeatureAccessManager::NotifyCameraRollAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnCameraRollAccessChanged();
}

void MultideviceFeatureAccessManager::NotifyAppsAccessChanged() {
  for (auto& observer : observer_list_)
    observer.OnAppsAccessChanged();
}

void MultideviceFeatureAccessManager::
    NotifyFeatureSetupRequestSupportedChanged() {
  for (auto& observer : observer_list_)
    observer.OnFeatureSetupRequestSupportedChanged();
}

void MultideviceFeatureAccessManager::SetNotificationSetupOperationStatus(
    NotificationAccessSetupOperation::Status new_status) {
  DCHECK(IsNotificationSetupOperationInProgress());

  PA_LOG(INFO) << "Notification access setup flow - new status: " << new_status;

  for (auto& it : id_to_notification_operation_map_)
    it.second->NotifyNotificationStatusChanged(new_status);

  if (NotificationAccessSetupOperation::IsFinalStatus(new_status))
    id_to_notification_operation_map_.clear();
}

bool MultideviceFeatureAccessManager::IsNotificationSetupOperationInProgress()
    const {
  return !id_to_notification_operation_map_.empty();
}

void MultideviceFeatureAccessManager::OnNotificationSetupOperationDeleted(
    int operation_id) {
  auto it = id_to_notification_operation_map_.find(operation_id);
  if (it == id_to_notification_operation_map_.end())
    return;

  id_to_notification_operation_map_.erase(it);

  if (id_to_notification_operation_map_.empty())
    PA_LOG(INFO) << "Notification access setup operation has ended.";
}

void MultideviceFeatureAccessManager::SetCombinedSetupOperationStatus(
    CombinedAccessSetupOperation::Status new_status) {
  DCHECK(IsCombinedSetupOperationInProgress());

  PA_LOG(INFO) << "Combined access setup flow - new status: " << new_status;

  for (auto& it : id_to_combined_operation_map_)
    it.second->NotifyCombinedStatusChanged(new_status);

  if (CombinedAccessSetupOperation::IsFinalStatus(new_status))
    id_to_combined_operation_map_.clear();
}

void MultideviceFeatureAccessManager::SetFeatureSetupConnectionOperationStatus(
    FeatureSetupConnectionOperation::Status new_status) {
  DCHECK(IsFeatureSetupConnectionOperationInProgress());

  PA_LOG(INFO) << "Feature setup connection status - new status: "
               << new_status;

  for (auto& it : id_to_connection_operation_map_)
    it.second->NotifyFeatureSetupConnectionStatusChanged(new_status);

  if (FeatureSetupConnectionOperation::IsFinalStatus(new_status))
    id_to_connection_operation_map_.clear();
}

bool MultideviceFeatureAccessManager::IsCombinedSetupOperationInProgress()
    const {
  return !id_to_combined_operation_map_.empty();
}

bool MultideviceFeatureAccessManager::
    IsFeatureSetupConnectionOperationInProgress() const {
  return !id_to_connection_operation_map_.empty();
}

void MultideviceFeatureAccessManager::OnNotificationSetupRequested() {}

void MultideviceFeatureAccessManager::OnCombinedSetupRequested(
    bool camera_roll,
    bool notifications) {}

void MultideviceFeatureAccessManager::OnFeatureSetupConnectionRequested() {}

void MultideviceFeatureAccessManager::
    UpdatedFeatureSetupConnectionStatusIfNeeded() {}

void MultideviceFeatureAccessManager::OnCombinedSetupOperationDeleted(
    int operation_id) {
  auto it = id_to_combined_operation_map_.find(operation_id);
  if (it == id_to_combined_operation_map_.end())
    return;

  id_to_combined_operation_map_.erase(it);

  if (id_to_combined_operation_map_.empty())
    PA_LOG(INFO) << "Combined access setup operation has ended.";
}

void MultideviceFeatureAccessManager::OnFeatureSetupConnectionOperationDeleted(
    int operation_id) {
  auto it = id_to_connection_operation_map_.find(operation_id);
  if (it == id_to_connection_operation_map_.end())
    return;

  id_to_connection_operation_map_.erase(it);

  if (id_to_connection_operation_map_.empty())
    PA_LOG(INFO) << "Feature setup connection operation has ended.";
}

void MultideviceFeatureAccessManager::Observer::OnNotificationAccessChanged() {
  // Optional method, inherit class doesn't have to implement this
}

void MultideviceFeatureAccessManager::Observer::OnCameraRollAccessChanged() {
  // Optional method, inherit class doesn't have to implement this
}

void MultideviceFeatureAccessManager::Observer::OnAppsAccessChanged() {
  // Optional method, inherit class doesn't have to implement this
}

void MultideviceFeatureAccessManager::Observer::
    OnFeatureSetupRequestSupportedChanged() {
  // Optional method, inherit class doesn't have to implement this
}

std::ostream& operator<<(std::ostream& stream,
                         MultideviceFeatureAccessManager::AccessStatus status) {
  switch (status) {
    case MultideviceFeatureAccessManager::AccessStatus::kProhibited:
      stream << "[Access prohibited]";
      break;
    case MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted:
      stream << "[Access available but not granted]";
      break;
    case MultideviceFeatureAccessManager::AccessStatus::kAccessGranted:
      stream << "[Access granted]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    MultideviceFeatureAccessManager::AccessProhibitedReason reason) {
  switch (reason) {
    case MultideviceFeatureAccessManager::AccessProhibitedReason::kUnknown:
      stream << "[Unknown]";
      break;
    case MultideviceFeatureAccessManager::AccessProhibitedReason::kWorkProfile:
      stream << "[Work Profile]";
      break;
    case MultideviceFeatureAccessManager::AccessProhibitedReason::
        kDisabledByPhonePolicy:
      stream << "[Admin Policy]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    std::pair<MultideviceFeatureAccessManager::AccessStatus,
              MultideviceFeatureAccessManager::AccessProhibitedReason>
        status_reason) {
  stream << status_reason.first;
  if (status_reason.first ==
      MultideviceFeatureAccessManager::AccessStatus::kProhibited) {
    stream << "," << status_reason.second;
  }
  return stream;
}

}  // namespace phonehub
}  // namespace ash
