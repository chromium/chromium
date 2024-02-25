// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_idle_observer.h"

namespace user_education {

namespace test {
class FeaturePromoSessionTestUtil;
}

class FeaturePromoIdlePolicy;
class FeaturePromoStorageService;

// Governs sessions for user education. May use cues such as application open
// and close times as well as active and inactive periods to determine when a
// session should start or end.
class FeaturePromoSessionManager {
 public:
  using IdleState = FeaturePromoIdleObserver::IdleState;

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

  // Determines whether the application is active. Inactive applications should
  // not show promos. Default is always active as long as the application is
  // running; override to modify behavior.
  bool IsApplicationActive() const;

 protected:
  const FeaturePromoIdlePolicy* idle_policy() const {
    return idle_policy_.get();
  }

  // Called when a new session is started. By default, does nothing. Override to
  // (for example) log metrics, or notify observers.
  virtual void OnNewSession(const base::Time old_start_time,
                            const base::Time old_active_time,
                            const base::Time new_active_time);

  // Called whenever the idle state is updated, before the session data is
  // updated in the storage service, so that both the current update and the
  // previous state can be used. By default, does nothing. Override to (for
  // example) log metrics.
  virtual void OnIdleStateUpdating(const IdleState& new_idle_state);

 private:
  void RecordActivePeriodDuration(base::TimeDelta duration);
  void RecordIdlePeriodDuration(base::TimeDelta duration);

  friend class FeaturePromoSessionManagerTest;
  friend test::FeaturePromoSessionTestUtil;

  void UpdateIdleState(const IdleState& new_idle_state);

  // Tracks whether the most recent update said the application was active or
  // not (i.e. due to the screen being locked, or no browser window being in the
  // foreground).
  bool application_is_active_ = true;

  raw_ptr<FeaturePromoStorageService> storage_service_ = nullptr;
  std::unique_ptr<FeaturePromoIdleObserver> idle_observer_;
  base::CallbackListSubscription idle_observer_subscription_;
  std::unique_ptr<FeaturePromoIdlePolicy> idle_policy_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_
