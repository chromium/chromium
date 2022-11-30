// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance_update.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

// static
void InstanceUpdate::Merge(Instance* state, const Instance* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }

  if ((delta->AppId() != state->AppId()) ||
      delta->InstanceId() != state->InstanceId()) {
    LOG(ERROR) << "inconsistent (app_id, instance_id): (" << delta->AppId()
               << ", " << delta->InstanceId() << ") vs (" << state->AppId()
               << ", " << state->InstanceId() << ") ";
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
  if (delta->Window()) {
    state->SetWindow(delta->Window());
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
      delta->InstanceId() != state->InstanceId()) {
    LOG(ERROR) << "inconsistent (app_id, instance_id): (" << delta->AppId()
               << ", " << delta->InstanceId() << ") vs (" << state->AppId()
               << ", " << state->InstanceId() << ") ";
    DCHECK(false);
    return false;
  }

  if (delta->Window() && delta->Window() != state->Window()) {
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

InstanceUpdate::InstanceUpdate(const Instance* state, const Instance* delta)
    : state_(state), delta_(delta) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK(state_->AppId() == delta->AppId());
    DCHECK(state_->InstanceId() == delta->InstanceId());
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

const base::UnguessableToken& InstanceUpdate::InstanceId() const {
  return delta_ ? delta_->InstanceId() : state_->InstanceId();
}

aura::Window* InstanceUpdate::Window() const {
  GET_VALUE(Window);
}

bool InstanceUpdate::WindowChanged() const {
  IS_VALUE_CHANGED(Window);
}

const std::string& InstanceUpdate::LaunchId() const {
  GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(LaunchId(), empty,
                                          base::EmptyString());
}

bool InstanceUpdate::LaunchIdChanged() const {
  IS_VALUE_CHANGED_WITH_CHECK(LaunchId(), empty);
}

InstanceState InstanceUpdate::State() const {
  GET_VALUE_WITH_DEFAULT_VALUE(State(), InstanceState::kUnknown);
}

bool InstanceUpdate::StateChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(State(), InstanceState::kUnknown);
}

base::Time InstanceUpdate::LastUpdatedTime() const {
  GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(LastUpdatedTime(), is_null,
                                          base::Time());
}

bool InstanceUpdate::LastUpdatedTimeChanged() const {
  IS_VALUE_CHANGED_WITH_CHECK(LastUpdatedTime(), is_null);
}

content::BrowserContext* InstanceUpdate::BrowserContext() const {
  GET_VALUE(BrowserContext);
}

bool InstanceUpdate::BrowserContextChanged() const {
  IS_VALUE_CHANGED(BrowserContext);
}

}  // namespace apps
