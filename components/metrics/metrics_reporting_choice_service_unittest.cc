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
    MetricsReportingChoiceService::RegisterPrefs(prefs_.registry());
    // Register the legacy pref as well, as it's not registered by
    // MetricsReportingChoiceService::RegisterPrefs but used in fallback.
    prefs_.registry()->RegisterBooleanPref(prefs::kMetricsReportingEnabled,
                                           false);
    MetricsReportingChoiceService::ClearCachedFeatureStateForTesting();
  }

  void TearDown() override {
    MetricsReportingChoiceService::ClearCachedFeatureStateForTesting();
  }

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

TEST_F(MetricsReportingChoiceServiceTest,
       FeatureStateTakesPreviousSessionValue_EnabledToDisabled) {
  feature_list_.InitAndDisableFeature(
      features::kRestructureMetricsConsentSettings);
  prefs_.SetBoolean(prefs::kMetricsConsentRestructureFeatureState, true);

  // Called before InitSyntheticFieldTrial, so it should read the previous
  // session's state from the pref.
  EXPECT_TRUE(
      MetricsReportingChoiceService::IsMetricsConsentRestructureFeatureEnabled(
          &prefs_));

  MetricsReportingChoiceService::InitSyntheticFieldTrial(&prefs_, &registry_);

  // State should remain true for the rest of the session despite feature being
  // disabled.
  EXPECT_TRUE(
      MetricsReportingChoiceService::IsMetricsConsentRestructureFeatureEnabled(
          &prefs_));
}

TEST_F(MetricsReportingChoiceServiceTest,
       FeatureStateTakesPreviousSessionValue_DisabledToEnabled) {
  feature_list_.InitAndEnableFeature(
      features::kRestructureMetricsConsentSettings);
  prefs_.SetBoolean(prefs::kMetricsConsentRestructureFeatureState, false);

  // Called before InitSyntheticFieldTrial, so it should read the previous
  // session's state from the pref.
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsMetricsConsentRestructureFeatureEnabled(
          &prefs_));

  MetricsReportingChoiceService::InitSyntheticFieldTrial(&prefs_, &registry_);

  // State should remain false for the rest of the session despite feature being
  // enabled.
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsMetricsConsentRestructureFeatureEnabled(
          &prefs_));
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
