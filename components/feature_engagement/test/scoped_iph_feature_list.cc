// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/test/scoped_iph_feature_list.h"

#include "base/check_op.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/internal/tracker_impl.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_engagement::test {

ScopedIphFeatureList::ScopedIphFeatureList() = default;

ScopedIphFeatureList::~ScopedIphFeatureList() {
  Reset();
}

void ScopedIphFeatureList::InitWithNoFeaturesAllowed() {
  InitWithExistingFeatures({});
}

void ScopedIphFeatureList::InitWithExistingFeatures(
    const std::vector<base::test::FeatureRef>& allow_features) {
  CHECK(!active_);

  std::vector<const base::Feature*> features;
  for (auto& ref : allow_features) {
    features.push_back(&*ref);
  }

  InitCommon(features);
}

void ScopedIphFeatureList::InitAndEnableFeatures(
    const std::vector<base::test::FeatureRef>& allow_and_enable_features,
    const std::vector<base::test::FeatureRef>& disable_features) {
  CHECK(!active_);

  feature_list_.InitWithFeatures(allow_and_enable_features, disable_features);

  std::vector<const base::Feature*> features;
  for (auto& ref : allow_and_enable_features) {
    features.push_back(&*ref);
  }

  InitCommon(features);
}

void ScopedIphFeatureList::InitAndEnableFeaturesWithParameters(
    const std::vector<base::test::FeatureRefAndParams>&
        allow_and_enable_features,
    const std::vector<base::test::FeatureRef>& disable_features) {
  CHECK(!active_);

  feature_list_.InitWithFeaturesAndParameters(allow_and_enable_features,
                                              disable_features);

  std::vector<const base::Feature*> features;
  for (auto& ref : allow_and_enable_features) {
    features.push_back(&ref.feature.get());
  }

  InitCommon(features);
}

void ScopedIphFeatureList::InitForDemo(
    const base::Feature& feature,
    const std::vector<base::test::FeatureRefAndParams>& additional_features) {
  std::vector<base::test::FeatureRefAndParams> actual_features =
      additional_features;
  actual_features.emplace_back(
      kIPHDemoMode,
      base::FieldTrialParams{{kIPHDemoModeFeatureChoiceParam, feature.name}});
  actual_features.emplace_back(feature, base::FieldTrialParams{});
  InitAndEnableFeaturesWithParameters(actual_features);
}

void ScopedIphFeatureList::Reset() {
  if (!active_) {
    return;
  }

  active_ = false;

  auto& data = TrackerImpl::GetAllowedTestFeatureMap();

  // This is the "total number of scopes" refcount.
  CHECK_GT(data[nullptr], 0U);
  --data[nullptr];

  // Decrement the individual feature refcounts for each feature that was
  // previously added.
  for (auto* feature : added_features_) {
    CHECK_GT(data[feature], 0U);
    --data[feature];
  }
  added_features_.clear();

  // Reset the feature list in case any features were enabled.
  feature_list_.Reset();
}

void ScopedIphFeatureList::InitCommon(
    const std::vector<const base::Feature*>& features) {
  CHECK(!active_);
  CHECK(added_features_.empty());

  active_ = true;

  auto& data = TrackerImpl::GetAllowedTestFeatureMap();

  // Increment the total number of scopes.
  ++data[nullptr];

  // Increment the refcount for each unique feature and add it to the saved
  // list for when they will be removed.
  for (auto* feature : features) {
    if (added_features_.insert(feature).second) {
      ++data[feature];
    }
  }
}

}  // namespace feature_engagement::test
