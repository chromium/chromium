// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/session/user_education_idle_observer.h"

#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

// Monitors the idle state of the current program/computer using various low-
// level APIs.
UserEducationIdleObserver::UserEducationIdleObserver() = default;
UserEducationIdleObserver::~UserEducationIdleObserver() = default;

void UserEducationIdleObserver::Init(
    const UserEducationTimeProvider* time_provider) {
  time_provider_ = time_provider;
}

void UserEducationIdleObserver::StartObserving() {}

base::CallbackListSubscription UserEducationIdleObserver::AddUpdateCallback(
    UpdateCallback update_callback) {
  return update_callbacks_.Add(std::move(update_callback));
}

void UserEducationIdleObserver::NotifyLastActiveChanged(base::Time update) {
  update_callbacks_.Notify(update);
}

base::Time UserEducationIdleObserver::GetCurrentTime() const {
  return time_provider_->GetCurrentTime();
}

}  // namespace user_education
