// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_TEST_UTIL_H_
#define COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_TEST_UTIL_H_

#include "base/memory/raw_ref.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"

namespace user_education {
struct FeaturePromoPolicyData;
struct FeaturePromoSessionData;
class FeaturePromoSessionManager;
}  // namespace user_education

namespace user_education::test {

// Class which seizes control of a `FeaturePromoSessionManager` and its
// associated session data, and gives a test fine control over the clock and the
// active state of the current session, simulating active and inactive periods
// as well as .
//
// This object should not outlive the session manager itself. Also, once this
// object has been attached to a session manager, it will no longer receive
// normal session updated, even after this object is destroyed, to avoid
// spurious events occurring at the end of a test or during teardown.
class FeaturePromoSessionTestUtil {
 public:
  FeaturePromoSessionTestUtil(FeaturePromoSessionManager& session_manager,
                              const FeaturePromoSessionData& session_data,
                              const FeaturePromoPolicyData& policy_data,
                              base::Time new_now);
  virtual ~FeaturePromoSessionTestUtil();

  FeaturePromoSessionTestUtil(const FeaturePromoSessionTestUtil&) = delete;
  void operator=(const FeaturePromoSessionTestUtil&) = delete;

  void SetNow(base::Time new_now);
  void UpdateIdleState(base::Time last_active_time, bool screen_locked);

 private:
  const raw_ref<FeaturePromoSessionManager> session_manager_;
  base::SimpleTestClock clock_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_TEST_UTIL_H_
