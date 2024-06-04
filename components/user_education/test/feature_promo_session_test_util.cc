// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/feature_promo_session_test_util.h"
#include <memory>

#include "base/callback_list.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/test/feature_promo_session_mocks.h"

namespace user_education::test {

FeaturePromoSessionTestUtil::FeaturePromoSessionTestUtil(
    FeaturePromoSessionManager& session_manager,
    const FeaturePromoSessionData& session_data,
    const FeaturePromoPolicyData& policy_data,
    std::optional<base::Time> new_last_active_time,
    std::optional<base::Time> new_now)
    : session_manager_(session_manager),
      storage_service_(*session_manager.storage_service_for_testing()) {
  if (new_now) {
    clock_ = std::make_unique<base::SimpleTestClock>();
    clock_->SetNow(*new_now);
    storage_service_->set_clock_for_testing(clock_.get());
  }
  storage_service_->SaveSessionData(session_data);
  storage_service_->SavePolicyData(policy_data);

  // Have to do this so that promos aren't blocked by the new session grace
  // period (at least by default). Set this to be a fairly mature profile,
  // active for an entire year.
  storage_service_->set_profile_creation_time_for_testing(
      storage_service_->GetCurrentTime() - base::Days(365));

  // Have to do this last so that the update happens after the session data is
  // saved.
  idle_observer_ = session_manager.ReplaceIdleObserverForTesting(
      std::make_unique<TestIdleObserver>(new_last_active_time));
}

FeaturePromoSessionTestUtil::~FeaturePromoSessionTestUtil() {
  // Have to set this back to avoid a dangling reference.
  session_manager_->storage_service_for_testing()->set_clock_for_testing(
      base::DefaultClock::GetInstance());
}

void FeaturePromoSessionTestUtil::SetNow(base::Time new_now) {
  clock_->SetNow(new_now);
}

void FeaturePromoSessionTestUtil::UpdateLastActiveTime(
    std::optional<base::Time> new_active_time,
    bool send_update) {
  idle_observer_->SetLastActiveTime(new_active_time, send_update);
}

}  // namespace user_education::test
