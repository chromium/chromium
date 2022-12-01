// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/find_my_device_controller_impl.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"

namespace ash {
namespace phonehub {

FindMyDeviceControllerImpl::FindMyDeviceControllerImpl(
    MessageSender* message_sender,
    UserActionRecorder* user_action_recorder)
    : message_sender_(message_sender),
      user_action_recorder_(user_action_recorder) {
  DCHECK(message_sender_);
}

FindMyDeviceControllerImpl::~FindMyDeviceControllerImpl() = default;

void FindMyDeviceControllerImpl::SetPhoneRingingStatusInternal(Status status) {
  if (phone_ringing_status_ == status)
    return;

  PA_LOG(INFO) << "Find My Device ringing status update: "
               << phone_ringing_status_ << " => " << status;
  phone_ringing_status_ = status;

  NotifyPhoneRingingStateChanged();
}

FindMyDeviceController::Status
FindMyDeviceControllerImpl::GetPhoneRingingStatus() {
  return phone_ringing_status_;
}

void FindMyDeviceControllerImpl::RequestNewPhoneRingingState(bool ringing) {
  if (phone_ringing_status_ == Status::kRingingNotAvailable) {
    PA_LOG(WARNING) << "Cannot request new ringing status because DoNotDisturb "
                    << "mode is enabled.";
    return;
  }

  PA_LOG(INFO) << "Attempting to set Find My Device phone ring state; new "
               << "value: " << ringing;
  user_action_recorder_->RecordFindMyDeviceAttempt();
  message_sender_->SendRingDeviceRequest(ringing);
}

}  // namespace phonehub
}  // namespace ash
