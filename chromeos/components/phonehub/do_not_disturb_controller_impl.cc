// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/do_not_disturb_controller_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/message_sender.h"

namespace chromeos {
namespace phonehub {

DoNotDisturbControllerImpl::DoNotDisturbControllerImpl(
    MessageSender* message_sender)
    : message_sender_(message_sender) {
  DCHECK(message_sender_);
}

DoNotDisturbControllerImpl::~DoNotDisturbControllerImpl() = default;

bool DoNotDisturbControllerImpl::IsDndEnabled() const {
  return is_dnd_enabled_;
}

void DoNotDisturbControllerImpl::SetDoNotDisturbStateInternal(
    bool is_dnd_enabled) {
  if (is_dnd_enabled == is_dnd_enabled_)
    return;

  is_dnd_enabled_ = is_dnd_enabled;
  NotifyDndStateChanged();
}

void DoNotDisturbControllerImpl::RequestNewDoNotDisturbState(bool enabled) {
  if (enabled == is_dnd_enabled_)
    return;

  PA_LOG(INFO) << "Attempting to set DND state; new value: " << enabled;
  message_sender_->SendUpdateNotificationModeRequest(enabled);
}

}  // namespace phonehub
}  // namespace chromeos
