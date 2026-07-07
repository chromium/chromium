// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_features.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

TEST(MetricsFeaturesTest, EnableNoopRuntimeMutableFeatures) {
  // Reset the cached values of the test features to ensure they are not
  // flagged as enabled from previous tests in the same process.
  base::FeatureList::ClearFeatureCachedValueForTesting(
      features::kNoopRuntimeMutableFeatureDefaultEnabled);
  base::FeatureList::ClearFeatureCachedValueForTesting(
      features::kNoopRuntimeMutableFeatureVariationsEnabled);

  // Initially, they should not have runtime mutability enabled.
  EXPECT_FALSE(features::kNoopRuntimeMutableFeatureDefaultEnabled
                   .HasRuntimeMutabilityEnabled());
  EXPECT_FALSE(features::kNoopRuntimeMutableFeatureVariationsEnabled
                   .HasRuntimeMutabilityEnabled());

  // Register the features.
  auto feature_list = std::make_unique<base::FeatureList>();
  features::EnableNoopRuntimeMutableFeatures(feature_list.get());

  // Verify that they now have runtime mutability enabled.
  EXPECT_TRUE(features::kNoopRuntimeMutableFeatureDefaultEnabled
                  .HasRuntimeMutabilityEnabled());
  EXPECT_TRUE(features::kNoopRuntimeMutableFeatureVariationsEnabled
                  .HasRuntimeMutabilityEnabled());
}

}  // namespace
}  // namespace metrics
