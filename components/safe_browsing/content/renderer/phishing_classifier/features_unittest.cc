// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(PhishingFeaturesTest, TooManyFeatures) {
  FeatureMap features;
  for (size_t i = 0; i < FeatureMap::kMaxFeatureMapSize; ++i) {
    EXPECT_TRUE(
        features.AddBooleanFeature(base::StringPrintf("Feature%" PRIuS, i)));
  }
  EXPECT_EQ(FeatureMap::kMaxFeatureMapSize, features.features().size());

  // Attempting to add more features should fail.
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_FALSE(
        features.AddBooleanFeature(base::StringPrintf("Extra%" PRIuS, i)));
  }
  EXPECT_EQ(FeatureMap::kMaxFeatureMapSize, features.features().size());
}

TEST(PhishingFeaturesTest, IllegalFeatureValue) {
  FeatureMap features;
  EXPECT_FALSE(features.AddRealFeature("toosmall", -0.1));
  EXPECT_TRUE(features.AddRealFeature("zero", 0.0));
  EXPECT_TRUE(features.AddRealFeature("pointfive", 0.5));
  EXPECT_TRUE(features.AddRealFeature("one", 1.0));
  EXPECT_FALSE(features.AddRealFeature("toolarge", 1.1));

  FeatureMap expected_features;
  expected_features.AddRealFeature("zero", 0.0);
  expected_features.AddRealFeature("pointfive", 0.5);
  expected_features.AddRealFeature("one", 1.0);
  ExpectFeatureMapsAreEqual(features, expected_features);
}

}  // namespace safe_browsing
