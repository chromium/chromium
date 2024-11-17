// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_SESSION_USER_EDUCATION_IDLE_OBSERVER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_SESSION_USER_EDUCATION_IDLE_OBSERVER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace user_education {

class UserEducationTimeProvider;

// Used to observe the system/application idle state. Override virtual methods
// for testing.
class UserEducationIdleObserver {
 public:
  using UpdateCallback = base::RepeatingCallback<void(base::Time)>;

  UserEducationIdleObserver();
  UserEducationIdleObserver(const UserEducationIdleObserver&) = delete;
  void operator=(const UserEducationIdleObserver&) = delete;
  virtual ~UserEducationIdleObserver();

  // Called by UserEducationSessionManager to initialize required data members.
  void Init(const UserEducationTimeProvider* time_provider);

  // Start any observation that is required to detect idle state changes.
  // Default is no-op.
  virtual void StartObserving();

  // Returns a new last active time for the application if there is one;
  // `nullopt` otherwise.
  //
  // Should always return `nullopt` if the application is inactive, the system
  // is locked, etc.
  virtual std::optional<base::Time> MaybeGetNewLastActiveTime() const = 0;

  // Adds a callback to get when the last active time is updated.
  base::CallbackListSubscription AddUpdateCallback(
      UpdateCallback update_callback);

 protected:
  // Sends notifications that the last active time has changed.
  void NotifyLastActiveChanged(base::Time update);

  // Gets the current time from the current time source.
  base::Time GetCurrentTime() const;

 private:
  base::RepeatingCallbackList<typename UpdateCallback::RunType>
      update_callbacks_;
  raw_ptr<const UserEducationTimeProvider> time_provider_ = nullptr;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_SESSION_USER_EDUCATION_IDLE_OBSERVER_H_
