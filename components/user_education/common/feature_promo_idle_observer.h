// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IDLE_OBSERVER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IDLE_OBSERVER_H_

#include <ostream>
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace user_education {

class FeaturePromoStorageService;

// Used to observe the system/application idle state. Override virtual methods
// for testing.
class FeaturePromoIdleObserver {
 public:
  // Describes the current idle state of the machine.
  struct IdleState {
    // The last known time the computer was active.
    base::Time last_active_time;

    // Whether the current application is active. This can take into account
    // things like whether the screen is locked, whether a specific window is
    // active, etc.
    bool application_active = true;

    // Allow member comparison for testing purposes.
    auto operator<=>(const IdleState&) const = default;
  };

  using UpdateCallback = base::RepeatingCallback<void(const IdleState&)>;

  FeaturePromoIdleObserver();
  FeaturePromoIdleObserver(const FeaturePromoIdleObserver&) = delete;
  void operator=(const FeaturePromoIdleObserver&) = delete;
  virtual ~FeaturePromoIdleObserver();

  // Called by FeaturePromoSessionManager to initialize required data members.
  void Init(const FeaturePromoStorageService* storage_service);

  // Start any observation that is required to detect idle state changes.
  // Default is no-op.
  virtual void StartObserving();

  // Returns the current idle state. Used on startup and shutdown.
  virtual IdleState GetCurrentState() const;

  // Adds a callback to get when the idle state is updated.
  base::CallbackListSubscription AddUpdateCallback(
      UpdateCallback update_callback);

 protected:
  // Sends notifications that the idle state has changed.
  void NotifyIdleStateChanged(const IdleState& state);

  // Gets the current time from the current time source.
  base::Time GetCurrentTime() const;

  base::RepeatingCallbackList<typename UpdateCallback::RunType>
      update_callbacks_;
  raw_ptr<const FeaturePromoStorageService> storage_service_ = nullptr;
};

std::ostream& operator<<(std::ostream& os,
                         const FeaturePromoIdleObserver::IdleState& idle_state);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IDLE_OBSERVER_H_
