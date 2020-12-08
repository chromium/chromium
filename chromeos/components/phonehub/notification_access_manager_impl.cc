// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_access_manager_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/connection_scheduler.h"
#include "chromeos/components/phonehub/message_sender.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace phonehub {

// static
void NotificationAccessManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kNotificationAccessStatus,
      static_cast<int>(AccessStatus::kAvailableButNotGranted));
  registry->RegisterBooleanPref(prefs::kHasDismissedSetupRequiredUi, false);
}

NotificationAccessManagerImpl::NotificationAccessManagerImpl(
    PrefService* pref_service,
    FeatureStatusProvider* feature_status_provider,
    MessageSender* message_sender,
    ConnectionScheduler* connection_scheduler)
    : pref_service_(pref_service),
      feature_status_provider_(feature_status_provider),
      message_sender_(message_sender),
      connection_scheduler_(connection_scheduler) {
  DCHECK(feature_status_provider_);
  DCHECK(message_sender_);

  current_feature_status_ = feature_status_provider_->GetStatus();
  feature_status_provider_->AddObserver(this);
}

NotificationAccessManagerImpl::~NotificationAccessManagerImpl() {
  feature_status_provider_->RemoveObserver(this);
}

bool NotificationAccessManagerImpl::HasNotificationSetupUiBeenDismissed()
    const {
  return pref_service_->GetBoolean(prefs::kHasDismissedSetupRequiredUi);
}

void NotificationAccessManagerImpl::DismissSetupRequiredUi() {
  pref_service_->SetBoolean(prefs::kHasDismissedSetupRequiredUi, true);
}

NotificationAccessManagerImpl::AccessStatus
NotificationAccessManagerImpl::GetAccessStatus() const {
  int status = pref_service_->GetInteger(prefs::kNotificationAccessStatus);
  return static_cast<AccessStatus>(status);
}

void NotificationAccessManagerImpl::SetAccessStatusInternal(
    AccessStatus access_status) {
  if (access_status == GetAccessStatus())
    return;

  PA_LOG(INFO) << "Notification access: " << GetAccessStatus() << " => "
               << access_status;

  pref_service_->SetInteger(prefs::kNotificationAccessStatus,
                            static_cast<int>(access_status));
  NotifyNotificationAccessChanged();

  if (!IsSetupOperationInProgress())
    return;

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
}

void NotificationAccessManagerImpl::OnSetupRequested() {
  PA_LOG(INFO) << "Notification access setup flow started.";

  switch (feature_status_provider_->GetStatus()) {
    // We're already connected, so request that the UI be shown on the phone.
    case FeatureStatus::kEnabledAndConnected:
      SendShowNotificationAccessSetupRequest();
      break;
    // We're already connecting, so wait until a connection succeeds before
    // trying to send a message
    case FeatureStatus::kEnabledAndConnecting:
      break;
    // We are not connected, so schedule a connection; once the
    // connection succeeds, we'll send the message in OnFeatureStatusChanged().
    case FeatureStatus::kEnabledButDisconnected:
      connection_scheduler_->ScheduleConnectionNow();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void NotificationAccessManagerImpl::OnFeatureStatusChanged() {
  if (!IsSetupOperationInProgress())
    return;

  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

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

void NotificationAccessManagerImpl::SendShowNotificationAccessSetupRequest() {
  message_sender_->SendShowNotificationAccessSetupRequest();
  SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status::
          kSentMessageToPhoneAndWaitingForResponse);
}

}  // namespace phonehub
}  // namespace chromeos
