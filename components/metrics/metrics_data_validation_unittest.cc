// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_data_validation.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

TEST(MetricsDataValidationTest, TestGetPseudoMetricsSampleNumeric) {
  const double sample = 100;
  {
    base::test::ScopedFeatureList scoped_feature_list;

    // When the feature is not enabled, |sample| should not be changed.
    EXPECT_DOUBLE_EQ(GetPseudoMetricsSample(sample), sample);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    // Small effect size.
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        internal::kPseudoMetricsEffectFeature,
        {{"multiplicative_factor", "1.02"}});

    // Added a small effect size. Make sure it relects on the pseudo sample.
    EXPECT_DOUBLE_EQ(GetPseudoMetricsSample(sample), 102);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    // Add Big effect size and additive factor.
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        internal::kPseudoMetricsEffectFeature,
        {{"multiplicative_factor", "1.10"}, {"additive_factor", "5"}});

    // Added a big effect size and additive factor. Make sure it relects on the
    // pseudo sample.
    EXPECT_DOUBLE_EQ(GetPseudoMetricsSample(sample), 115);
  }
}

TEST(MetricsDataValidationTest, TestGetPseudoMetricsSampleTimeDelta) {
  // Make sure this also works for time metrics.
  const base::TimeDelta sample = base::Milliseconds(100);
  {
    base::test::ScopedFeatureList scoped_feature_list;

    EXPECT_EQ(GetPseudoMetricsSample(sample), sample);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    // Small effect size.
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        internal::kPseudoMetricsEffectFeature,
        {{"multiplicative_factor", "1.02"}});

    EXPECT_EQ(GetPseudoMetricsSample(sample), base::Milliseconds(102));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    // Big effect size.
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        internal::kPseudoMetricsEffectFeature,
        {{"multiplicative_factor", "1.10"}, {"additive_factor", "5"}});

    EXPECT_EQ(GetPseudoMetricsSample(sample), base::Milliseconds(115));
  }
}

}  // namespace

}  // namespace metrics
