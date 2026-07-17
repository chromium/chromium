// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_choice_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class MetricsReportingChoiceServiceTest : public testing::Test {
 protected:
  MetricsReportingChoiceServiceTest() {
    // Register the legacy pref, as it's not registered by
    // MetricsReportingChoiceService but used in fallback.
    prefs_.registry()->RegisterBooleanPref(prefs::kMetricsReportingEnabled,
                                           false);
  }

  void TearDown() override {}

  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList feature_list_;
  variations::SyntheticTrialRegistry registry_;
};

TEST_F(MetricsReportingChoiceServiceTest, IsBasicMetricsReportingEnabled) {
  prefs_.SetBoolean(prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(
      MetricsReportingChoiceService::IsBasicMetricsReportingEnabled(&prefs_));

  prefs_.SetBoolean(prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsBasicMetricsReportingEnabled(&prefs_));
}

TEST_F(MetricsReportingChoiceServiceTest, FeatureState) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kRestructureMetricsConsentSettings);
    EXPECT_TRUE(MetricsReportingChoiceService::
                    IsMetricsConsentRestructureFeatureEnabled());
    EXPECT_TRUE(
        MetricsReportingChoiceService::ShouldUseMetricsConsentRestructure());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kRestructureMetricsConsentSettings);
    EXPECT_FALSE(MetricsReportingChoiceService::
                     IsMetricsConsentRestructureFeatureEnabled());
    EXPECT_FALSE(
        MetricsReportingChoiceService::ShouldUseMetricsConsentRestructure());
  }
}

TEST_F(MetricsReportingChoiceServiceTest, IsMetricsReportingDisabledByPolicy) {
  // Not managed, disabled.
  prefs_.SetBoolean(prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsMetricsReportingDisabledByPolicy(
          &prefs_));

  // Managed, enabled.
  prefs_.SetManagedPref(prefs::kMetricsReportingEnabled, base::Value(true));
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsMetricsReportingDisabledByPolicy(
          &prefs_));

  // Managed, disabled.
  prefs_.SetManagedPref(prefs::kMetricsReportingEnabled, base::Value(false));
  EXPECT_TRUE(MetricsReportingChoiceService::IsMetricsReportingDisabledByPolicy(
      &prefs_));
}

}  // namespace metrics
