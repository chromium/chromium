// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_TEST_UTIL_H_
#define COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_TEST_UTIL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"

namespace user_education {
struct FeaturePromoPolicyData;
struct FeaturePromoSessionData;
class FeaturePromoSessionManager;
class FeaturePromoStorageService;
}  // namespace user_education

namespace user_education::test {

class TestIdleObserver;

// Class which seizes control of a `FeaturePromoSessionManager` and its
// associated session data, and gives a test fine control over the clock and the
// active state of the current session, simulating active and inactive periods.
//
// This object should not outlive the session manager itself. Also, once this
// object has been attached to a session manager, it will no longer receive
// normal session updated, even after this object is destroyed, to avoid
// spurious events occurring at the end of a test or during teardown.
//
// USAGE NOTE: for browser-based tests, prefer using
// `InteractiveFeaturePromoTest[T]` instead of directly using this class.
class FeaturePromoSessionTestUtil {
 public:
  // Creates the util object and seizes control of the session manager.
  // If `new_now` is set, also replaces the system clock with a test clock.
  FeaturePromoSessionTestUtil(FeaturePromoSessionManager& session_manager,
                              const FeaturePromoSessionData& session_data,
                              const FeaturePromoPolicyData& policy_data,
                              std::optional<base::Time> new_last_active_time,
                              std::optional<base::Time> new_now);
  virtual ~FeaturePromoSessionTestUtil();

  FeaturePromoSessionTestUtil(const FeaturePromoSessionTestUtil&) = delete;
  void operator=(const FeaturePromoSessionTestUtil&) = delete;

  // Returns the current time, whether from the test or "real" clock.
  base::Time Now() const { return clock_ ? clock_->Now() : base::Time::Now(); }

  // Sets the current time if using a test clock; fails if using the real clock.
  void SetNow(base::Time new_now);
  void UpdateLastActiveTime(std::optional<base::Time> new_active_time,
                            bool send_update);

 private:
  const raw_ref<FeaturePromoSessionManager> session_manager_;
  const raw_ref<FeaturePromoStorageService> storage_service_;
  raw_ptr<TestIdleObserver> idle_observer_ = nullptr;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_TEST_UTIL_H_
