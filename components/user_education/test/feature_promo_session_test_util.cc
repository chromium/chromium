// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/feature_promo_session_test_util.h"

#include "base/callback_list.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_storage_service.h"

namespace user_education::test {

FeaturePromoSessionTestUtil::FeaturePromoSessionTestUtil(
    FeaturePromoSessionManager& session_manager,
    const FeaturePromoSessionData& session_data,
    const FeaturePromoPolicyData& policy_data,
    base::Time new_now)
    : session_manager_(session_manager) {
  clock_.SetNow(new_now);
  session_manager_->storage_service_->set_clock_for_testing(&clock_);
  session_manager_->storage_service_->SaveSessionData(session_data);
  session_manager_->storage_service_->SavePolicyData(policy_data);
  session_manager_->is_locked_ = false;

  // Unsubscribe from the current idle poller and eliminate it.
  session_manager_->idle_observer_subscription_ =
      base::CallbackListSubscription();
  session_manager_->idle_observer_.reset();
}

FeaturePromoSessionTestUtil::~FeaturePromoSessionTestUtil() {
  // Have to set this back to avoid a dangling reference.
  session_manager_->storage_service_->set_clock_for_testing(
      base::DefaultClock::GetInstance());
}

void FeaturePromoSessionTestUtil::SetNow(base::Time new_now) {
  clock_.SetNow(new_now);
}

void FeaturePromoSessionTestUtil::UpdateIdleState(base::Time last_active_time,
                                                  bool screen_locked) {
  session_manager_->UpdateIdleState(
      FeaturePromoSessionManager::IdleState{last_active_time, screen_locked});
}

}  // namespace user_education::test
