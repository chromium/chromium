// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/do_not_disturb_controller_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace phonehub {

DoNotDisturbControllerImpl::DoNotDisturbControllerImpl() = default;

DoNotDisturbControllerImpl::~DoNotDisturbControllerImpl() = default;

bool DoNotDisturbControllerImpl::IsDndEnabled() const {
  return is_dnd_enabled_;
}

void DoNotDisturbControllerImpl::SetDoNotDisturbStateInternal(
    bool is_dnd_enabled) {
  is_dnd_enabled_ = is_dnd_enabled;
}

void DoNotDisturbControllerImpl::RequestNewDoNotDisturbState(bool enabled) {
  PA_LOG(INFO) << "Attempting to set DND state; new value: " << enabled;
}

}  // namespace phonehub
}  // namespace chromeos
