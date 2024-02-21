// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_

#include <algorithm>
#include <memory>
#include <ostream>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace user_education {

namespace test {
class FeaturePromoSessionTestUtil;
}

class FeaturePromoStorageService;

// Governs sessions for user education. May use cues such as application open
// and close times as well as active and inactive periods to determine when a
// session should start or end.
class FeaturePromoSessionManager {
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

  // Used to observe the system/application idle state. Override virtual methods
  // for testing.
  class IdleObserver {
   public:
    using UpdateCallback = base::RepeatingCallback<void(const IdleState&)>;

    IdleObserver();
    IdleObserver(const IdleObserver&) = delete;
    void operator=(const IdleObserver&) = delete;
    virtual ~IdleObserver();

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

   private:
    friend FeaturePromoSessionManager;

    base::RepeatingCallbackList<typename UpdateCallback::RunType>
        update_callbacks_;
    raw_ptr<const FeaturePromoSessionManager> session_manager_ = nullptr;
  };

  // Used to determine when the session is active or not based on periods of
  // idle and active time. Currently implements the v2 behavior. OVerride
  // virtual methods for testing.
  class IdlePolicy {
   public:
    // Construct an idle policy with values pulled from the v2 flag, or defaults
    // if the flag is not set.
    IdlePolicy();
    IdlePolicy(const IdlePolicy&) = delete;
    void operator=(const IdlePolicy&) = delete;
    virtual ~IdlePolicy();

    // Determines if the session is currently active based on the last time the
    // application was active.
    virtual bool IsActive(base::Time most_recent_active_time) const;

    // Determines if a new session should start based on the start of the last
    // session, the last time the application was active, and the new active
    // start time. Only call if `IsActive()` returns true; a session cannot
    // start when the application is inactive.
    virtual bool IsNewSession(base::Time previous_session_start_time,
                              base::Time previous_last_active_time,
                              base::Time most_recent_active_time) const;

    // Sets the idle time resolution. This function may be called multiple times
    // but the value set must be the same.
    static void SetIdleTimeResolution(base::TimeDelta resolution);

   protected:
    // Constructs the idle policy with explicit values for each of the
    // thresholds.
    IdlePolicy(base::TimeDelta minimum_idle_time,
               base::TimeDelta new_session_idle_time,
               base::TimeDelta minimum_valid_session_length);

    base::TimeDelta minimum_idle_time() const {
      return std::max(minimum_idle_time_,
                      idle_time_resolution_ * kIdleTimeResolutionFactor);
    }
    base::TimeDelta new_session_idle_time() const {
      return new_session_idle_time_;
    }
    base::TimeDelta minimum_valid_session_length() const {
      return minimum_valid_session_length_;
    }

   private:
    friend FeaturePromoSessionManager;
    friend class FeaturePromoSessionManagerIdlePolicyTest;

    // This value can be set elsewhere in code to set the minimum known time
    // resolution for idle time. Unset means unknown. The actual time used to
    // determine if the browser is idle is
    //   max(minimum_idle_time_,
    //       idle_time_resolution_ * kIdleTimeResolutionFactor)
    static base::TimeDelta idle_time_resolution_;
    static constexpr double kIdleTimeResolutionFactor = 1.5;

    // The minimum length of time since the last activity before the
    // application is considered idle. Must be nonzero since the sampling of
    // activity is necessarily coarse.
    const base::TimeDelta minimum_idle_time_;

    // The minimum amount of time the application must remain idle before new
    // activity is considered a new session. Must be nonzero.
    const base::TimeDelta new_session_idle_time_;

    // The minimum length of a session; if a previous session lasted for less
    // than this amount of time before the application became idle again then
    // the old session can be discarded and a new one started immediately.
    const base::TimeDelta minimum_valid_session_length_;

    raw_ptr<const FeaturePromoSessionManager> session_manager_ = nullptr;
  };

  FeaturePromoSessionManager();
  FeaturePromoSessionManager(const FeaturePromoSessionManager&) = delete;
  void operator=(const FeaturePromoSessionManager&) = delete;
  virtual ~FeaturePromoSessionManager();

  // Initialize the session manager. If overridden, the base class must be
  // called. In derived classes, internal data should be set up or restored
  // either in the constructor or before calling this version of Init.
  virtual void Init(FeaturePromoStorageService* storage_service,
                    std::unique_ptr<IdleObserver> observer,
                    std::unique_ptr<IdlePolicy> policy);

  // Determines whether the application is active. Inactive applications should
  // not show promos. Default is always active as long as the application is
  // running; override to modify behavior.
  bool IsApplicationActive() const;

 protected:
  const IdlePolicy* idle_policy() const { return idle_policy_.get(); }

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
  std::unique_ptr<IdleObserver> idle_observer_;
  base::CallbackListSubscription idle_observer_subscription_;
  std::unique_ptr<IdlePolicy> idle_policy_;
};

std::ostream& operator<<(
    std::ostream& os,
    const FeaturePromoSessionManager::IdleState& idle_state);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_H_
