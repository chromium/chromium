// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter::features {
namespace {

class FingerprintingProtectionFeaturesTest : public ::testing::Test {
 public:
  FingerprintingProtectionFeaturesTest() = default;
  void SetUp() override {}

  void TearDown() override { scoped_feature_list_.Reset(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FingerprintingProtectionFeaturesTest,
       PerformancemanceMeasurementRateSet_NonIncognito_MeasurePerformance) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"performance_measurement_rate", "1.0"}});

  EXPECT_EQ(SampleEnablePerformanceMeasurements(/*is_incognito=*/false), true);
}

TEST_F(FingerprintingProtectionFeaturesTest,
       PerformancemanceMeasurementRateSet_Incognito_MeasurePerformance) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      {{"performance_measurement_rate", "1.0"}});

  EXPECT_EQ(SampleEnablePerformanceMeasurements(/*is_incognito=*/true), true);
}

TEST_F(
    FingerprintingProtectionFeaturesTest,
    PerformancemanceMeasurementRateNotSet_NonIncognito_DoNotMeasurePerformance) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter, {{}});

  EXPECT_EQ(SampleEnablePerformanceMeasurements(/*is_incognito=*/false), false);
}

TEST_F(
    FingerprintingProtectionFeaturesTest,
    PerformancemanceMeasurementRateNotSet_Incognito_DoNotMeasurePerformance) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter, {{}});

  EXPECT_EQ(SampleEnablePerformanceMeasurements(/*is_incognito=*/true), false);
}

}  // namespace
}  // namespace fingerprinting_protection_filter::features
