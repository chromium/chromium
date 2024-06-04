// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/pref_names.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/connection_scheduler.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "pref_names.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

}  // namespace

// static
void MultideviceFeatureAccessManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kCameraRollAccessStatus,
      static_cast<int>(AccessStatus::kAvailableButNotGranted));
  registry->RegisterIntegerPref(
      prefs::kNotificationAccessStatus,
      static_cast<int>(AccessStatus::kAvailableButNotGranted));
  registry->RegisterIntegerPref(
      prefs::kNotificationAccessProhibitedReason,
      static_cast<int>(AccessProhibitedReason::kUnknown));
  registry->RegisterBooleanPref(prefs::kHasDismissedSetupRequiredUi, false);
  registry->RegisterBooleanPref(prefs::kNeedsOneTimeNotificationAccessUpdate,
                                true);
  registry->RegisterBooleanPref(prefs::kFeatureSetupRequestSupported, false);
}

MultideviceFeatureAccessManagerImpl::MultideviceFeatureAccessManagerImpl(
    PrefService* pref_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    FeatureStatusProvider* feature_status_provider,
    MessageSender* message_sender,
    ConnectionScheduler* connection_scheduler)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      feature_status_provider_(feature_status_provider),
      message_sender_(message_sender),
      connection_scheduler_(connection_scheduler) {
  DCHECK(feature_status_provider_);
  DCHECK(message_sender_);
  DCHECK(multidevice_setup_client_);

  current_feature_status_ = feature_status_provider_->GetStatus();
  PA_LOG(VERBOSE) << __func__
                  << ": current feature status = " << current_feature_status_;

  feature_status_provider_->AddObserver(this);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      eche_app::prefs::kAppsAccessStatus,
      base::BindRepeating(
          &MultideviceFeatureAccessManagerImpl::NotifyAppsAccessChanged,
          base::Unretained(this)));
}

MultideviceFeatureAccessManagerImpl::~MultideviceFeatureAccessManagerImpl() {
  feature_status_provider_->RemoveObserver(this);
  pref_change_registrar_.RemoveAll();
}

bool MultideviceFeatureAccessManagerImpl::
    HasMultideviceFeatureSetupUiBeenDismissed() const {
  return pref_service_->GetBoolean(prefs::kHasDismissedSetupRequiredUi);
}

void MultideviceFeatureAccessManagerImpl::DismissSetupRequiredUi() {
  pref_service_->SetBoolean(prefs::kHasDismissedSetupRequiredUi, true);
}

bool MultideviceFeatureAccessManagerImpl::IsAccessRequestAllowed(
    Feature feature) {
  const FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(feature);
  bool result = feature_state == FeatureState::kDisabledByUser ||
                feature_state == FeatureState::kEnabledByUser;
  return result;
}

MultideviceFeatureAccessManagerImpl::AccessStatus
MultideviceFeatureAccessManagerImpl::GetNotificationAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kNotificationAccessStatus);
  return static_cast<AccessStatus>(status);
}

MultideviceFeatureAccessManagerImpl::AccessStatus
MultideviceFeatureAccessManagerImpl::GetCameraRollAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kCameraRollAccessStatus);
  return static_cast<AccessStatus>(status);
}

MultideviceFeatureAccessManager::AccessStatus
MultideviceFeatureAccessManagerImpl::GetAppsAccessStatus() const {
  // TODO(samchiu): The AppsAccessStatus will be updated by eche_app_ui
  // component only. We should listen to pref change and update it to
  // MultiDeviceFeatureOptInView.
  int status = pref_service_->GetInteger(eche_app::prefs::kAppsAccessStatus);
  return static_cast<AccessStatus>(status);
}

bool MultideviceFeatureAccessManagerImpl::GetFeatureSetupRequestSupported()
    const {
  return pref_service_->GetBoolean(prefs::kFeatureSetupRequestSupported);
}

MultideviceFeatureAccessManagerImpl::AccessProhibitedReason
MultideviceFeatureAccessManagerImpl::GetNotificationAccessProhibitedReason()
    const {
  int reason =
      pref_service_->GetInteger(prefs::kNotificationAccessProhibitedReason);
  return static_cast<AccessProhibitedReason>(reason);
}

void MultideviceFeatureAccessManagerImpl::SetNotificationAccessStatusInternal(
    AccessStatus access_status,
    AccessProhibitedReason reason) {
  // TODO(http://crbug.com/1215559): Deprecate when there are no more active
  // Phone Hub notification users on M89. Some users had notifications
  // automatically disabled when updating from M89 to M90+ because the
  // notification feature state went from enabled-by-default to
  // disabled-by-default. To re-enable those users, we once and only once notify
  // observers if access has been granted by the phone. Notably, the
  // MultideviceSetupStateUpdate will decide whether or not the notification
  // feature should be enabled. See MultideviceSetupStateUpdater's method
  // IsWaitingForAccessToInitiallyEnableNotifications() for more details.
  bool needs_one_time_notifications_access_update =
      pref_service_->GetBoolean(prefs::kNeedsOneTimeNotificationAccessUpdate) &&
      access_status == AccessStatus::kAccessGranted;

  if (!needs_one_time_notifications_access_update &&
      !HasAccessStatusChanged(access_status, reason)) {
    return;
  }

  pref_service_->SetBoolean(prefs::kNeedsOneTimeNotificationAccessUpdate,
                            false);

  PA_LOG(INFO) << "Notification access: "
               << std::make_pair(GetNotificationAccessStatus(),
                                 GetNotificationAccessProhibitedReason())
               << " => " << std::make_pair(access_status, reason);

  pref_service_->SetInteger(prefs::kNotificationAccessStatus,
                            static_cast<int>(access_status));
  pref_service_->SetInteger(prefs::kNotificationAccessProhibitedReason,
                            static_cast<int>(reason));
  NotifyNotificationAccessChanged();

  if (IsNotificationSetupOperationInProgress()) {
    switch (access_status) {
      case AccessStatus::kProhibited:
        SetNotificationSetupOperationStatus(
            NotificationAccessSetupOperation::Status::
                kProhibitedFromProvidingAccess);
        break;
      case AccessStatus::kAccessGranted:
        SetNotificationSetupOperationStatus(
            NotificationAccessSetupOperation::Status::kCompletedSuccessfully);
        break;
      case AccessStatus::kAvailableButNotGranted:
        // Intentionally blank; the operation status should not change.
        break;
    }
  } else if (IsCombinedSetupOperationInProgress()) {
    switch (access_status) {
      case AccessStatus::kProhibited:
        SetCombinedSetupOperationStatus(CombinedAccessSetupOperation::Status::
                                            kProhibitedFromProvidingAccess);
        break;
      case AccessStatus::kAccessGranted:
        combined_setup_notifications_pending_ = false;
        break;
      case AccessStatus::kAvailableButNotGranted:
        // Intentionally blank; the operation status should not change.
        break;
    }
    if (!combined_setup_notifications_pending_ &&
        !combined_setup_camera_roll_pending_) {
      SetCombinedSetupOperationStatus(
          CombinedAccessSetupOperation::Status::kCompletedSuccessfully);
    }
  }
}

void MultideviceFeatureAccessManagerImpl::SetCameraRollAccessStatusInternal(
    AccessStatus access_status) {
  PA_LOG(INFO) << "Camera Roll access: " << GetCameraRollAccessStatus()
               << " => " << access_status;
  pref_service_->SetInteger(prefs::kCameraRollAccessStatus,
                            static_cast<int>(access_status));
  NotifyCameraRollAccessChanged();

  if (!IsCombinedSetupOperationInProgress()) {
    return;
  }

  switch (access_status) {
    case AccessStatus::kProhibited:
      SetCombinedSetupOperationStatus(
          CombinedAccessSetupOperation::Status::kProhibitedFromProvidingAccess);
      break;
    case AccessStatus::kAccessGranted:
      combined_setup_camera_roll_pending_ = false;
      break;
    case AccessStatus::kAvailableButNotGranted:
      // Intentionally blank; the operation status should not change.
      break;
  }
  if (!combined_setup_notifications_pending_ &&
      !combined_setup_camera_roll_pending_) {
    SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::kCompletedSuccessfully);
  }
}

void MultideviceFeatureAccessManagerImpl::
    SetFeatureSetupRequestSupportedInternal(bool supported) {
  pref_service_->SetBoolean(prefs::kFeatureSetupRequestSupported, supported);
  NotifyFeatureSetupRequestSupportedChanged();
}

void MultideviceFeatureAccessManagerImpl::OnNotificationSetupRequested() {
  PA_LOG(INFO) << "Notification access setup flow started.";

  switch (feature_status_provider_->GetStatus()) {
    // We're already connected, so request that the UI be shown on the phone.
    case FeatureStatus::kEnabledAndConnected:
      SendShowNotificationAccessSetupRequest();
      break;
    // We're already connecting, so wait until a connection succeeds before
    // trying to send a message
    case FeatureStatus::kEnabledAndConnecting:
      SetNotificationSetupOperationStatus(
          NotificationAccessSetupOperation::Status::kConnecting);
      break;
    // We are not connected, so schedule a connection; once the
    // connection succeeds, we'll send the message in OnFeatureStatusChanged().
    case FeatureStatus::kEnabledButDisconnected:
      SetNotificationSetupOperationStatus(
          NotificationAccessSetupOperation::Status::kConnecting);
      connection_scheduler_->ScheduleConnectionNow(
          phonehub::DiscoveryEntryPoint::kMultiDeviceFeatureSetup);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void MultideviceFeatureAccessManagerImpl::OnCombinedSetupRequested(
    bool camera_roll,
    bool notifications) {
  combined_setup_camera_roll_pending_ = camera_roll;
  combined_setup_notifications_pending_ = notifications;
  PA_LOG(INFO) << "Combined access setup flow started.";

  switch (feature_status_provider_->GetStatus()) {
    // We're already connected, so request that the UI be shown on the phone.
    case FeatureStatus::kEnabledAndConnected:
      SendShowCombinedAccessSetupRequest();
      break;
    // We're already connecting, so wait until a connection succeeds before
    // trying to send a message
    case FeatureStatus::kEnabledAndConnecting:
      SetCombinedSetupOperationStatus(
          CombinedAccessSetupOperation::Status::kConnecting);
      break;
    // We are not connected, so schedule a connection; once the
    // connection succeeds, we'll send the message in OnFeatureStatusChanged().
    case FeatureStatus::kEnabledButDisconnected:
      SetCombinedSetupOperationStatus(
          CombinedAccessSetupOperation::Status::kConnecting);
      connection_scheduler_->ScheduleConnectionNow(
          DiscoveryEntryPoint::kMultiDeviceFeatureSetup);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void MultideviceFeatureAccessManagerImpl::OnFeatureSetupConnectionRequested() {
  PA_LOG(INFO) << "Connection for feature setup started";

  switch (feature_status_provider_->GetStatus()) {
    case FeatureStatus::kEnabledAndConnected:
      SetFeatureSetupConnectionOperationStatus(
          FeatureSetupConnectionOperation::Status::kConnected);
      break;
    case FeatureStatus::kEnabledAndConnecting:
      SetFeatureSetupConnectionOperationStatus(
          FeatureSetupConnectionOperation::Status::kConnecting);
      break;
    case FeatureStatus::kEnabledButDisconnected:
    case FeatureStatus::kUnavailableBluetoothOff:
      SetFeatureSetupConnectionOperationStatus(
          FeatureSetupConnectionOperation::Status::kConnecting);
      connection_scheduler_->ScheduleConnectionNow(
          DiscoveryEntryPoint::kMultiDeviceFeatureSetup);
      break;
    default:
      DUMP_WILL_BE_NOTREACHED();
      break;
  }
}

void MultideviceFeatureAccessManagerImpl::OnFeatureStatusChanged() {
  if (IsFeatureSetupConnectionOperationInProgress()) {
    FeatureStatusChangedFeatureSetupConnection();
  } else if (IsNotificationSetupOperationInProgress()) {
    FeatureStatusChangedNotificationAccessSetup();
  } else if (IsCombinedSetupOperationInProgress()) {
    FeatureStatusChangedCombinedAccessSetup();
  }
}

void MultideviceFeatureAccessManagerImpl::
    FeatureStatusChangedNotificationAccessSetup() {
  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  PA_LOG(VERBOSE) << __func__
                  << ": previous feature status = " << previous_feature_status
                  << ", current feature status = " << current_feature_status_;

  if (previous_feature_status == current_feature_status_)
    return;

  // If we were previously connecting and could not establish a connection,
  // send a timeout state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnecting &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetNotificationSetupOperationStatus(
        NotificationAccessSetupOperation::Status::kTimedOutConnecting);
    return;
  }

  // If we were previously connected and are now no longer connected, send a
  // connection disconnected state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnected &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetNotificationSetupOperationStatus(
        NotificationAccessSetupOperation::Status::kConnectionDisconnected);
    return;
  }

  if (current_feature_status_ == FeatureStatus::kEnabledAndConnected) {
    SendShowNotificationAccessSetupRequest();
    return;
  }
}

void MultideviceFeatureAccessManagerImpl::
    FeatureStatusChangedCombinedAccessSetup() {
  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  PA_LOG(VERBOSE) << __func__
                  << ": previous feature status = " << previous_feature_status
                  << ", current feature status = " << current_feature_status_;

  if (previous_feature_status == current_feature_status_)
    return;

  // If we were previously connecting and could not establish a connection,
  // send a timeout state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnecting &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::kTimedOutConnecting);
    return;
  }

  // If we were previously connected and are now no longer connected, send a
  // connection disconnected state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnected &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetCombinedSetupOperationStatus(
        CombinedAccessSetupOperation::Status::kConnectionDisconnected);
    return;
  }

  if (current_feature_status_ == FeatureStatus::kEnabledAndConnected) {
    SendShowCombinedAccessSetupRequest();
    return;
  }
}

void MultideviceFeatureAccessManagerImpl::
    FeatureStatusChangedFeatureSetupConnection() {
  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  PA_LOG(VERBOSE) << __func__
                  << ": previous feature status = " << previous_feature_status
                  << ", current feature status = " << current_feature_status_;

  if (previous_feature_status == current_feature_status_)
    return;

  // If we were previously connecting and could not establish a connection,
  // send a timeout state.
  if (previous_feature_status == FeatureStatus::kEnabledAndConnecting &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetFeatureSetupConnectionOperationStatus(
        FeatureSetupConnectionOperation::Status::kTimedOutConnecting);
    return;
  }

  if (previous_feature_status == FeatureStatus::kEnabledAndConnected &&
      current_feature_status_ != FeatureStatus::kEnabledAndConnected) {
    SetFeatureSetupConnectionOperationStatus(
        FeatureSetupConnectionOperation::Status::kConnectionLost);
    return;
  }

  if (current_feature_status_ == FeatureStatus::kEnabledAndConnected) {
    feature_setup_connection_update_pending_ = true;
    return;
  }
}

void MultideviceFeatureAccessManagerImpl::
    UpdatedFeatureSetupConnectionStatusIfNeeded() {
  if (feature_setup_connection_update_pending_) {
    SetFeatureSetupConnectionOperationStatus(
        FeatureSetupConnectionOperation::Status::kConnected);
  }
  feature_setup_connection_update_pending_ = false;
}

void MultideviceFeatureAccessManagerImpl::
    SendShowNotificationAccessSetupRequest() {
  message_sender_->SendShowNotificationAccessSetupRequest();
  SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status::
          kSentMessageToPhoneAndWaitingForResponse);
}

void MultideviceFeatureAccessManagerImpl::SendShowCombinedAccessSetupRequest() {
  message_sender_->SendFeatureSetupRequest(
      combined_setup_camera_roll_pending_,
      combined_setup_notifications_pending_);
  SetCombinedSetupOperationStatus(CombinedAccessSetupOperation::Status::
                                      kSentMessageToPhoneAndWaitingForResponse);
}

bool MultideviceFeatureAccessManagerImpl::HasAccessStatusChanged(
    AccessStatus access_status,
    AccessProhibitedReason reason) {
  if (access_status != GetNotificationAccessStatus())
    return true;
  if (access_status == AccessStatus::kProhibited &&
      reason != GetNotificationAccessProhibitedReason()) {
    return true;
  }
  return false;
}

}  // namespace phonehub
}  // namespace ash
