// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/do_not_disturb_controller_impl.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"

namespace ash {
namespace phonehub {

DoNotDisturbControllerImpl::DoNotDisturbControllerImpl(
    MessageSender* message_sender,
    UserActionRecorder* user_action_recorder)
    : message_sender_(message_sender),
      user_action_recorder_(user_action_recorder) {
  DCHECK(message_sender_);
}

DoNotDisturbControllerImpl::~DoNotDisturbControllerImpl() = default;

bool DoNotDisturbControllerImpl::IsDndEnabled() const {
  return is_dnd_enabled_;
}

void DoNotDisturbControllerImpl::SetDoNotDisturbStateInternal(
    bool is_dnd_enabled,
    bool can_request_new_dnd_state) {
  if (is_dnd_enabled_ == is_dnd_enabled &&
      can_request_new_dnd_state_ == can_request_new_dnd_state) {
    return;
  }

  if (is_dnd_enabled != is_dnd_enabled_) {
    PA_LOG(INFO) << "Do Not Disturb state updated: " << is_dnd_enabled_
                 << " => " << is_dnd_enabled;
    is_dnd_enabled_ = is_dnd_enabled;
  }

  if (can_request_new_dnd_state != can_request_new_dnd_state_) {
    PA_LOG(INFO) << "Can request new Do Not Disturb state updated: "
                 << can_request_new_dnd_state_ << " => "
                 << can_request_new_dnd_state;
    can_request_new_dnd_state_ = can_request_new_dnd_state;
  }

  NotifyDndStateChanged();
}

void DoNotDisturbControllerImpl::RequestNewDoNotDisturbState(bool enabled) {
  if (enabled == is_dnd_enabled_)
    return;

  PA_LOG(INFO) << "Attempting to set DND state; new value: " << enabled;
  user_action_recorder_->RecordDndAttempt();
  message_sender_->SendUpdateNotificationModeRequest(enabled);
}

bool DoNotDisturbControllerImpl::CanRequestNewDndState() const {
  return can_request_new_dnd_state_;
}

}  // namespace phonehub
}  // namespace ash
