// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/capability_access_update.h"

#include <ostream>

#include "base/check.h"
#include "base/logging.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

// static
void CapabilityAccessUpdate::Merge(CapabilityAccess* state,
                                   const CapabilityAccess* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }

  if (delta->app_id != state->app_id) {
    LOG(ERROR) << "inconsistent (app_id): (" << delta->app_id << ") vs ("
               << state->app_id << ") ";
    DCHECK(false);
    return;
  }

  SET_OPTIONAL_VALUE(camera);
  SET_OPTIONAL_VALUE(microphone);

  // When adding new fields to the CapabilityAccess Mojo type, this function
  // should also be updated.
}

CapabilityAccessUpdate::CapabilityAccessUpdate(const CapabilityAccess* state,
                                               const CapabilityAccess* delta,
                                               const ::AccountId& account_id)
    : state_(state), delta_(delta), account_id_(account_id) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK(state_->app_id == delta->app_id);
  }
}

bool CapabilityAccessUpdate::StateIsNull() const {
  return state_ == nullptr;
}

const std::string& CapabilityAccessUpdate::AppId() const {
  return delta_ ? delta_->app_id : state_->app_id;
}

std::optional<bool> CapabilityAccessUpdate::Camera() const {
  GET_VALUE_WITH_FALLBACK(camera, std::nullopt)
}

bool CapabilityAccessUpdate::CameraChanged() const {
    RETURN_OPTIONAL_VALUE_CHANGED(camera)}

std::optional<bool> CapabilityAccessUpdate::Microphone() const {
  GET_VALUE_WITH_FALLBACK(microphone, std::nullopt)
}

bool CapabilityAccessUpdate::MicrophoneChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(microphone)
}

const ::AccountId& CapabilityAccessUpdate::AccountId() const {
  return *account_id_;
}

bool CapabilityAccessUpdate::IsAccessingAnyCapability() const {
  return Camera().value_or(false) || Microphone().value_or(false);
}

std::ostream& operator<<(std::ostream& out,
                         const CapabilityAccessUpdate& update) {
  out << "AppId: " << update.AppId() << std::endl;
  out << "Camera: " << PRINT_OPTIONAL_BOOL(update.Camera()) << std::endl;
  out << "Microphone: " << PRINT_OPTIONAL_BOOL(update.Microphone())
      << std::endl;
  return out;
}

}  // namespace apps
