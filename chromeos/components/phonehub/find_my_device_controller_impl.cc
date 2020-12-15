// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/find_my_device_controller_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/message_sender.h"
#include "chromeos/components/phonehub/user_action_recorder.h"

namespace chromeos {
namespace phonehub {

FindMyDeviceControllerImpl::FindMyDeviceControllerImpl(
    DoNotDisturbController* do_not_disturb_controller,
    MessageSender* message_sender,
    UserActionRecorder* user_action_recorder)
    : do_not_disturb_controller_(do_not_disturb_controller),
      message_sender_(message_sender),
      user_action_recorder_(user_action_recorder) {
  DCHECK(do_not_disturb_controller_);
  DCHECK(message_sender_);

  do_not_disturb_controller_->AddObserver(this);
}

FindMyDeviceControllerImpl::~FindMyDeviceControllerImpl() {
  do_not_disturb_controller_->RemoveObserver(this);
}

void FindMyDeviceControllerImpl::SetIsPhoneRingingInternal(
    bool is_phone_ringing) {
  is_phone_ringing_ = is_phone_ringing;
  UpdateStatus();
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

void FindMyDeviceControllerImpl::OnDndStateChanged() {
  UpdateStatus();
}

FindMyDeviceController::Status FindMyDeviceControllerImpl::ComputeStatus()
    const {
  if (do_not_disturb_controller_->IsDndEnabled()) {
    PA_LOG(WARNING) << "Cannot set ringing status because DoNotDisturb mode is "
                    << "enabled.";
    return Status::kRingingNotAvailable;
  }
  return is_phone_ringing_ ? Status::kRingingOn : Status::kRingingOff;
}

void FindMyDeviceControllerImpl::UpdateStatus() {
  Status status = ComputeStatus();
  if (phone_ringing_status_ == status)
    return;

  PA_LOG(INFO) << "Find My Device ringing status update: "
               << phone_ringing_status_ << " => " << status;
  phone_ringing_status_ = status;

  NotifyPhoneRingingStateChanged();
}

}  // namespace phonehub
}  // namespace chromeos
