// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance_update.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/instance.h"

namespace apps {

// static
void InstanceUpdate::Merge(Instance* state, const Instance* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }
  if ((delta->AppId() != state->AppId()) ||
      delta->GetInstanceKey() != state->GetInstanceKey()) {
    LOG(ERROR) << "inconsistent (app_id, instance_key): (" << delta->AppId()
               << ", " << delta->GetInstanceKey() << ") vs (" << state->AppId()
               << ", " << state->GetInstanceKey() << ") ";
    DCHECK(false);
    return;
  }
  if (!delta->LaunchId().empty()) {
    state->SetLaunchId(delta->LaunchId());
  }
  if (delta->State() != InstanceState::kUnknown) {
    state->UpdateState(delta->State(), delta->LastUpdatedTime());
  }
  if (delta->BrowserContext()) {
    state->SetBrowserContext(delta->BrowserContext());
  }
  // When adding new fields to the Instance class, this function should also be
  // updated.
}

// static
bool InstanceUpdate::Equals(const Instance* state, const Instance* delta) {
  if (delta == nullptr) {
    return true;
  }
  if (state == nullptr) {
    if (static_cast<InstanceState>(delta->State() &
                                   InstanceState::kDestroyed) !=
        InstanceState::kUnknown) {
      return true;
    }
    return false;
  }

  if ((delta->AppId() != state->AppId()) ||
      delta->GetInstanceKey() != state->GetInstanceKey()) {
    LOG(ERROR) << "inconsistent (app_id, instance_key): (" << delta->AppId()
               << ", " << delta->GetInstanceKey() << ") vs (" << state->AppId()
               << ", " << state->GetInstanceKey() << ") ";
    DCHECK(false);
    return false;
  }
  if (!delta->LaunchId().empty() && delta->LaunchId() != state->LaunchId()) {
    return false;
  }
  if (delta->State() != InstanceState::kUnknown &&
      (delta->State() != state->State() ||
       delta->LastUpdatedTime() != state->LastUpdatedTime())) {
    return false;
  }
  if (delta->BrowserContext() &&
      delta->BrowserContext() != state->BrowserContext()) {
    return false;
  }
  return true;
  // When adding new fields to the Instance class, this function should also be
  // updated.
}

InstanceUpdate::InstanceUpdate(Instance* state, Instance* delta)
    : state_(state), delta_(delta) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK(state_->AppId() == delta->AppId());
    DCHECK(state_->GetInstanceKey() == delta->GetInstanceKey());
  }
}

bool InstanceUpdate::StateIsNull() const {
  return state_ == nullptr;
}

bool InstanceUpdate::IsCreation() const {
  return StateIsNull() && (State() & apps::InstanceState::kDestroyed) ==
                              apps::InstanceState::kUnknown;
}

bool InstanceUpdate::IsDestruction() const {
  return StateChanged() && State() == apps::InstanceState::kDestroyed;
}

const std::string& InstanceUpdate::AppId() const {
  return delta_ ? delta_->AppId() : state_->AppId();
}

aura::Window* InstanceUpdate::Window() const {
  return InstanceKey().Window();
}

const Instance::InstanceKey& InstanceUpdate::InstanceKey() const {
  return delta_ ? delta_->GetInstanceKey() : state_->GetInstanceKey();
}

const std::string& InstanceUpdate::LaunchId() const {
  if (delta_ && !delta_->LaunchId().empty()) {
    return delta_->LaunchId();
  }
  if (state_ && !state_->LaunchId().empty()) {
    return state_->LaunchId();
  }
  return base::EmptyString();
}

bool InstanceUpdate::LaunchIdChanged() const {
  return delta_ && !delta_->LaunchId().empty() &&
         (!state_ || (delta_->LaunchId() != state_->LaunchId()));
}

InstanceState InstanceUpdate::State() const {
  if (delta_ && (delta_->State() != InstanceState::kUnknown)) {
    return delta_->State();
  }
  if (state_) {
    return state_->State();
  }
  return InstanceState::kUnknown;
}

bool InstanceUpdate::StateChanged() const {
  return delta_ && (delta_->State() != InstanceState::kUnknown) &&
         (!state_ || (delta_->State() != state_->State()));
}

base::Time InstanceUpdate::LastUpdatedTime() const {
  if (delta_ && !delta_->LastUpdatedTime().is_null()) {
    return delta_->LastUpdatedTime();
  }
  if (state_ && !state_->LastUpdatedTime().is_null()) {
    return state_->LastUpdatedTime();
  }

  return base::Time();
}

bool InstanceUpdate::LastUpdatedTimeChanged() const {
  return delta_ && !delta_->LastUpdatedTime().is_null() &&
         (!state_ || (delta_->LastUpdatedTime() != state_->LastUpdatedTime()));
}

content::BrowserContext* InstanceUpdate::BrowserContext() const {
  if (delta_ && delta_->BrowserContext()) {
    return delta_->BrowserContext();
  }
  if (state_ && state_->BrowserContext()) {
    return state_->BrowserContext();
  }
  return nullptr;
}

bool InstanceUpdate::BrowserContextChanged() const {
  return delta_ && delta_->BrowserContext() &&
         (!state_ || (delta_->BrowserContext() != state_->BrowserContext()));
}

}  // namespace apps
