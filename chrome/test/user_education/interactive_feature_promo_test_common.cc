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

InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromosWithParams::
    UseDefaultTrackerAllowingPromosWithParams(
        std::vector<base::test::FeatureRefAndParams> features_with_params_,
        TrackerInitializationMode initialization_mode_)
    : features_with_params(std::move(features_with_params_)),
      initialization_mode(initialization_mode_) {
  for (const auto& feature_and_params : features_with_params) {
    for (const auto& param : feature_and_params.params) {
      CHECK(param.first.starts_with("x_"))
          << "IPH feature parameters must start with \"x_\" so the Feature "
             "Engagement system ignores them.";
    }
  }
}
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromosWithParams::
    UseDefaultTrackerAllowingPromosWithParams(
        UseDefaultTrackerAllowingPromosWithParams&&) noexcept = default;
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromosWithParams&
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromosWithParams::
operator=(UseDefaultTrackerAllowingPromosWithParams&&) noexcept = default;
InteractiveFeaturePromoTestCommon::UseDefaultTrackerAllowingPromosWithParams::
    ~UseDefaultTrackerAllowingPromosWithParams() = default;
