// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_idle_observer.h"

namespace user_education {

class FeaturePromoIdlePolicy;
class FeaturePromoStorageService;

// Governs sessions for user education. May use cues such as application open
// and close times as well as active and inactive periods to determine when a
// session should start or end.
class FeaturePromoSessionManager {
 public:
  FeaturePromoSessionManager();
  FeaturePromoSessionManager(const FeaturePromoSessionManager&) = delete;
  void operator=(const FeaturePromoSessionManager&) = delete;
  virtual ~FeaturePromoSessionManager();

  // Initialize the session manager. If overridden, the base class must be
  // called. In derived classes, internal data should be set up or restored
  // either in the constructor or before calling this version of Init.
  virtual void Init(FeaturePromoStorageService* storage_service,
                    std::unique_ptr<FeaturePromoIdleObserver> observer,
                    std::unique_ptr<FeaturePromoIdlePolicy> policy);

  // Possibly updates the current session state, if there might be a new
  // session.
  void MaybeUpdateSessionState();

  // Registers a callback that will be called whenever a new session happens.
  // To avoid a race condition at startup, you should also check
  // `new_session_since_startup()`.
  base::CallbackListSubscription AddNewSessionCallback(
      base::RepeatingClosure new_session_callback);

  // Returns whether there has been a new session since application startup.
  bool new_session_since_startup() const { return new_session_since_startup_; }

  // Test-only methods.
  FeaturePromoStorageService* storage_service_for_testing() {
    return storage_service_;
  }
  template <typename T>
    requires std::derived_from<T, FeaturePromoIdleObserver>
  T* ReplaceIdleObserverForTesting(std::unique_ptr<T> new_observer) {
    T* const observer_ptr = new_observer.get();
    SetIdleObserver(std::move(new_observer));
    return observer_ptr;
  }

 protected:
  const FeaturePromoIdlePolicy* idle_policy() const {
    return idle_policy_.get();
  }

  // Called when a new session is started. By default, does nothing. Override to
  // (for example) log metrics, or notify observers.
  virtual void OnNewSession(const base::Time old_start_time,
                            const base::Time old_active_time,
                            const base::Time new_active_time);

  // Called whenever the most recent active time is updated, before the session
  // data is updated in the storage service, so that both the current update and
  // the previous state can be used. By default, does nothing. Override to (for
  // example) log metrics.
  virtual void OnLastActiveTimeUpdating(base::Time new_last_active_time);

 private:
  void SetIdleObserver(std::unique_ptr<FeaturePromoIdleObserver> new_observer);

  void RecordActivePeriodDuration(base::TimeDelta duration);
  void RecordIdlePeriodDuration(base::TimeDelta duration);

  friend class FeaturePromoSessionManagerTest;

  void UpdateLastActiveTime(base::Time new_last_active_time);

  raw_ptr<FeaturePromoStorageService> storage_service_ = nullptr;
  std::unique_ptr<FeaturePromoIdleObserver> idle_observer_;
  base::CallbackListSubscription idle_observer_subscription_;
  std::unique_ptr<FeaturePromoIdlePolicy> idle_policy_;
  base::RepeatingCallbackList<void()> new_session_callbacks_;
  bool new_session_since_startup_ = false;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_
