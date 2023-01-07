// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_TEST_UTIL_H_
#define COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_TEST_UTIL_H_

#include "base/feature_list.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace user_education {

class FeaturePromoControllerCommon;

namespace test {

// Blocks until the feature engagement backend loads and returns whether the
// initialization succeeded.
//
// Eliminates common boilerplate in tests that use a live Feature Engagement
// Tracker rather than a mock (especially e.g. browser tests).
bool WaitForFeatureEngagementReady(feature_engagement::Tracker* tracker);

// Convenience method that uses a feature promo controller instead of a Tracker.
//
// If `controller` is null or does not have a tracker, returns false.
bool WaitForFeatureEngagementReady(FeaturePromoControllerCommon* controller);

// Waits for the FE system to start up and then verifies that the promo for
// `iph_feature` is shown. Will generate a test error on tracker initialization
// failure.
//
// Does not actually request for the promo to be shown; it is assumed that the
// test itself will set up the conditions for the promo.
bool WaitForStartupPromo(feature_engagement::Tracker* tracker,
                         const base::Feature& iph_feature);

// Convenience method that uses a feature promo controller instead of a Tracker.
//
// If `controller` is null or does not have a tracker, returns false.
bool WaitForStartupPromo(FeaturePromoControllerCommon* controller,
                         const base::Feature& iph_feature);

}  // namespace test
}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_TEST_UTIL_H_
