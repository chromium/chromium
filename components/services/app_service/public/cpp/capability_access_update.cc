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

  if (delta->camera.has_value()) {
    state->camera = delta->camera;
  }
  if (delta->microphone.has_value()) {
    state->microphone = delta->microphone;
  }
  // When adding new fields to the CapabilityAccess Mojo type, this function
  // should also be updated.
}

CapabilityAccessUpdate::CapabilityAccessUpdate(
    const apps::mojom::CapabilityAccess* state,
    const apps::mojom::CapabilityAccess* delta,
    const ::AccountId& account_id)
    : mojom_state_(state), mojom_delta_(delta), account_id_(account_id) {
  DCHECK(mojom_state_ || mojom_delta_);
  if (mojom_state_ && mojom_delta_) {
    DCHECK(mojom_state_->app_id == delta->app_id);
  }
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
  return mojom_state_ == nullptr;
}

const std::string& CapabilityAccessUpdate::AppId() const {
  return mojom_delta_ ? mojom_delta_->app_id : mojom_state_->app_id;
}

apps::mojom::OptionalBool CapabilityAccessUpdate::Camera() const {
  if (mojom_delta_ &&
      (mojom_delta_->camera != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->camera;
  }
  if (mojom_state_) {
    return mojom_state_->camera;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool CapabilityAccessUpdate::CameraChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->camera != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ || (mojom_delta_->camera != mojom_state_->camera));
}

apps::mojom::OptionalBool CapabilityAccessUpdate::Microphone() const {
  if (mojom_delta_ &&
      (mojom_delta_->microphone != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->microphone;
  }
  if (mojom_state_) {
    return mojom_state_->microphone;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool CapabilityAccessUpdate::MicrophoneChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->microphone != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->microphone != mojom_state_->microphone));
}

const ::AccountId& CapabilityAccessUpdate::AccountId() const {
  return account_id_;
}

}  // namespace apps
