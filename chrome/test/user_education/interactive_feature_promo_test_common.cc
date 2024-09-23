// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/user_education/interactive_feature_promo_test_common.h"

#include <vector>

#include "base/test/scoped_feature_list.h"

InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromos::
    UseDefaultTrackerAllowingPromos(
        std::vector<base::test::FeatureRef> features_,
        TrackerInitializationMode initialization_mode_)
    : features(std::move(features_)),
      initialization_mode(initialization_mode_) {}
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromos::
    UseDefaultTrackerAllowingPromos(
        UseDefaultTrackerAllowingPromos&&) noexcept = default;
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromos&
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromos::operator=(
    UseDefaultTrackerAllowingPromos&&) noexcept = default;
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromos::
    ~UseDefaultTrackerAllowingPromos() = default;
