// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/feature_promo_test_util.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education::test {

bool WaitForFeatureEngagementReady(feature_engagement::Tracker* tracker) {
  if (!tracker)
    return false;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  auto quit_closure = run_loop.QuitClosure();
  bool success = false;
  tracker->AddOnInitializedCallback(
      base::BindLambdaForTesting([&](bool initialized) {
        success = initialized;
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
  DCHECK(!success || tracker->IsInitialized());
  return success;
}

bool WaitForFeatureEngagementReady(FeaturePromoControllerCommon* controller) {
  return controller && WaitForFeatureEngagementReady(
                           controller->feature_engagement_tracker());
}

bool WaitForStartupPromo(feature_engagement::Tracker* tracker,
                         const base::Feature& iph_feature) {
  const bool fe_init_succeeded = WaitForFeatureEngagementReady(tracker);
  EXPECT_TRUE(fe_init_succeeded);
  return fe_init_succeeded &&
         tracker->GetTriggerState(iph_feature) ==
             feature_engagement::Tracker::TriggerState::HAS_BEEN_DISPLAYED;
}

bool WaitForStartupPromo(FeaturePromoControllerCommon* controller,
                         const base::Feature& iph_feature) {
  return controller &&
         WaitForStartupPromo(controller->feature_engagement_tracker(),
                             iph_feature);
}

bool SetClock(FeaturePromoControllerCommon* controller,
              const base::Clock& clock,
              base::Time initial_time) {
  if (!controller || !controller->feature_engagement_tracker()) {
    return false;
  }
  controller->feature_engagement_tracker()->SetClockForTesting(clock,
                                                               initial_time);
  return true;
}

}  // namespace user_education::test