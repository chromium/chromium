// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IDLE_POLICY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IDLE_POLICY_H_

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace user_education {

class FeaturePromoSessionManager;
class FeaturePromoStorageService;

// Used to determine when the session is active or not based on periods of
// idle and active time. Currently implements the v2 behavior. OVerride
// virtual methods for testing.
class FeaturePromoIdlePolicy {
 public:
  // Construct an idle policy with values pulled from the v2 flag, or defaults
  // if the flag is not set.
  FeaturePromoIdlePolicy();
  FeaturePromoIdlePolicy(const FeaturePromoIdlePolicy&) = delete;
  void operator=(const FeaturePromoIdlePolicy&) = delete;
  virtual ~FeaturePromoIdlePolicy();

  // Called by FeaturePromoSessionManager to initialize required data members.
  void Init(const FeaturePromoSessionManager* session_manager,
            const FeaturePromoStorageService* storage_service);

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
  FeaturePromoIdlePolicy(base::TimeDelta minimum_idle_time,
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
  friend class FeaturePromoIdlePolicyTest;

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
  raw_ptr<const FeaturePromoStorageService> storage_service_ = nullptr;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IDLE_POLICY_H_
