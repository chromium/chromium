// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/capability_access_update.h"

#include "base/logging.h"

namespace apps {

// static
void CapabilityAccessUpdate::Merge(apps::mojom::CapabilityAccess* state,
                                   const apps::mojom::CapabilityAccess* delta) {
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

  if (delta->camera != apps::mojom::OptionalBool::kUnknown) {
    state->camera = delta->camera;
  }
  if (delta->microphone != apps::mojom::OptionalBool::kUnknown) {
    state->microphone = delta->microphone;
  }
  // When adding new fields to the CapabilityAccess Mojo type, this function
  // should also be updated.
}

CapabilityAccessUpdate::CapabilityAccessUpdate(
    const apps::mojom::CapabilityAccess* state,
    const apps::mojom::CapabilityAccess* delta,
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

apps::mojom::OptionalBool CapabilityAccessUpdate::Camera() const {
  if (delta_ && (delta_->camera != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->camera;
  }
  if (state_) {
    return state_->camera;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool CapabilityAccessUpdate::CameraChanged() const {
  return delta_ && (delta_->camera != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->camera != state_->camera));
}

apps::mojom::OptionalBool CapabilityAccessUpdate::Microphone() const {
  if (delta_ && (delta_->microphone != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->microphone;
  }
  if (state_) {
    return state_->microphone;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool CapabilityAccessUpdate::MicrophoneChanged() const {
  return delta_ &&
         (delta_->microphone != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->microphone != state_->microphone));
}

const ::AccountId& CapabilityAccessUpdate::AccountId() const {
  return account_id_;
}

}  // namespace apps
