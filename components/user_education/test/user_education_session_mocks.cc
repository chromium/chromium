// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/user_education_session_mocks.h"

#include <optional>

namespace user_education::test {

TestIdleObserver::TestIdleObserver(
    std::optional<base::Time> initial_active_time)
    : last_active_time_(initial_active_time) {}
TestIdleObserver::~TestIdleObserver() = default;

void TestIdleObserver::SetLastActiveTime(
    std::optional<base::Time> new_last_active_time,
    bool send_update) {
  last_active_time_ = new_last_active_time;
  if (send_update) {
    CHECK(new_last_active_time);
    NotifyLastActiveChanged(*new_last_active_time);
  }
}

void TestIdleObserver::StartObserving() {}

std::optional<base::Time> TestIdleObserver::MaybeGetNewLastActiveTime() const {
  return last_active_time_;
}

MockIdlePolicy::MockIdlePolicy() = default;
MockIdlePolicy::~MockIdlePolicy() = default;

TestUserEducationSessionProvider::TestUserEducationSessionProvider(
    bool has_new_session)
    : has_new_session_(has_new_session) {}
TestUserEducationSessionProvider::~TestUserEducationSessionProvider() = default;

base::CallbackListSubscription
TestUserEducationSessionProvider::AddNewSessionCallback(
    base::RepeatingClosure new_session_callback) {
  return callbacks_.Add(std::move(new_session_callback));
}

bool TestUserEducationSessionProvider::GetNewSessionSinceStartup() const {
  return has_new_session_;
}

void TestUserEducationSessionProvider::StartNewSession() {
  has_new_session_ = true;
  callbacks_.Notify();
}

MockUserEducationSessionManager::MockUserEducationSessionManager() = default;
MockUserEducationSessionManager::~MockUserEducationSessionManager() = default;

}  // namespace user_education::test
