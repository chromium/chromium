// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/notification_manager_impl.h"

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"

namespace ash {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

NotificationManagerImpl::NotificationManagerImpl(
    MessageSender* message_sender,
    UserActionRecorder* user_action_recorder,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : message_sender_(message_sender),
      user_action_recorder_(user_action_recorder),
      multidevice_setup_client_(multidevice_setup_client) {
  DCHECK(message_sender_);
  DCHECK(multidevice_setup_client_);

  multidevice_setup_client_->AddObserver(this);
}

NotificationManagerImpl::~NotificationManagerImpl() {
  multidevice_setup_client_->RemoveObserver(this);
}

void NotificationManagerImpl::DismissNotification(int64_t notification_id) {
  PA_LOG(INFO) << "Dismissing notification with ID " << notification_id << ".";

  if (!GetNotification(notification_id)) {
    PA_LOG(WARNING) << "Attempted to dismiss an invalid notification with id: "
                    << notification_id << ".";
    return;
  }

  user_action_recorder_->RecordNotificationDismissAttempt();
  RemoveNotificationsInternal(base::flat_set<int64_t>{notification_id});
  message_sender_->SendDismissNotificationRequest(notification_id);
}

void NotificationManagerImpl::SendInlineReply(
    int64_t notification_id,
    const std::u16string& inline_reply_text) {
  if (!GetNotification(notification_id)) {
    PA_LOG(INFO) << "Could not send inline reply for notification with ID "
                 << notification_id << ".";
    return;
  }

  PA_LOG(INFO) << "Sending inline reply for notification with ID "
               << notification_id << ".";
  user_action_recorder_->RecordNotificationReplyAttempt();
  message_sender_->SendNotificationInlineReplyRequest(notification_id,
                                                      inline_reply_text);
}

void NotificationManagerImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  FeatureState notifications_feature_state =
      multidevice_setup_client_->GetFeatureState(
          Feature::kPhoneHubNotifications);
  if (notifications_feature_status_ == FeatureState::kEnabledByUser &&
      notifications_feature_state != FeatureState::kEnabledByUser) {
    ClearNotificationsInternal();
  }

  notifications_feature_status_ = notifications_feature_state;
}

}  // namespace phonehub
}  // namespace ash
