// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_choice_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_profile_pref_names.h"
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

TEST_F(MetricsReportingChoiceServiceTest, RegisterProfilePrefs) {
  TestingPrefServiceSimple prefs;
  MetricsReportingChoiceService::RegisterProfilePrefs(prefs.registry());
  EXPECT_FALSE(prefs.GetBoolean(prefs::kAdvancedReportingEnabled));
  EXPECT_FALSE(prefs.GetBoolean(prefs::kAdvancedReportingProfileMigrationDone));
}

TEST_F(MetricsReportingChoiceServiceTest, AdvancedReportingEnabled) {
  TestingPrefServiceSimple prefs;
  MetricsReportingChoiceService::RegisterProfilePrefs(prefs.registry());

  // Default value should be false.
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsAdvancedReportingEnabled(&prefs));

  // Set to true and verify.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs, true);
  EXPECT_TRUE(
      MetricsReportingChoiceService::IsAdvancedReportingEnabled(&prefs));

  // Set to false and verify.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs, false);
  EXPECT_FALSE(
      MetricsReportingChoiceService::IsAdvancedReportingEnabled(&prefs));
}

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
    EXPECT_TRUE(
        MetricsReportingChoiceService::ShouldUseMetricsConsentRestructure());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kRestructureMetricsConsentSettings);
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

class TestMetricsReportingChoiceService : public MetricsReportingChoiceService {
 public:
  TestMetricsReportingChoiceService() = default;
  ~TestMetricsReportingChoiceService() override = default;

  bool was_notified() const { return was_notified_; }
  bool last_enabled() const { return last_enabled_; }
  bool last_reset_client_state() const { return last_reset_client_state_; }
  void ResetNotification() {
    was_notified_ = false;
    last_enabled_ = false;
    last_reset_client_state_ = false;
  }

 protected:
  void OnAdvancedReportingEnabledForAllProfilesChanged(
      bool enabled,
      bool reset_client_state) override {
    was_notified_ = true;
    last_enabled_ = enabled;
    last_reset_client_state_ = reset_client_state;
  }

 private:
  bool was_notified_ = false;
  bool last_enabled_ = false;
  bool last_reset_client_state_ = false;
};

TEST_F(MetricsReportingChoiceServiceTest, MultiProfileMonitoring) {
  TestMetricsReportingChoiceService service;
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());

  TestingPrefServiceSimple prefs1;
  MetricsReportingChoiceService::RegisterProfilePrefs(prefs1.registry());
  TestingPrefServiceSimple prefs2;
  MetricsReportingChoiceService::RegisterProfilePrefs(prefs2.registry());

  // Monitor first profile (pref is default false).
  service.MonitorAdvancedReportingPref(&prefs1);
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_FALSE(service.was_notified());

  // Enable advanced reporting on first profile.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs1, true);
  EXPECT_TRUE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_TRUE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
  service.ResetNotification();

  // Monitor second profile (pref is default false).
  service.MonitorAdvancedReportingPref(&prefs2);
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_FALSE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
  service.ResetNotification();

  // Enable advanced reporting on second profile.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs2, true);
  EXPECT_TRUE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_TRUE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
  service.ResetNotification();

  // Disable advanced reporting on second profile (user opt-out).
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs2, false);
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_FALSE(service.last_enabled());
  EXPECT_TRUE(service.last_reset_client_state());
  service.ResetNotification();

  // Re-enable advanced reporting on second profile.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs2, true);
  EXPECT_TRUE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_TRUE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
  service.ResetNotification();

  // Stop monitoring first profile (should remain enabled since prefs2 is true).
  service.StopMonitoringAdvancedReportingPref(&prefs1);
  EXPECT_TRUE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_FALSE(service.was_notified());

  // Stop monitoring second profile (monitored profiles is now empty, so false).
  service.StopMonitoringAdvancedReportingPref(&prefs2);
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_FALSE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
}

TEST_F(MetricsReportingChoiceServiceTest,
       RevokeConsentWhileAggregatedStateIsDisabled) {
  TestMetricsReportingChoiceService service;
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());

  TestingPrefServiceSimple prefs1;
  MetricsReportingChoiceService::RegisterProfilePrefs(prefs1.registry());
  TestingPrefServiceSimple prefs2;
  MetricsReportingChoiceService::RegisterProfilePrefs(prefs2.registry());

  // Enable advanced reporting on first profile.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs1, true);

  // Monitor first profile (pref is true).
  service.MonitorAdvancedReportingPref(&prefs1);
  EXPECT_TRUE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_TRUE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
  service.ResetNotification();

  // Monitor second profile (pref is default false).
  // This changes the aggregated state to false.
  service.MonitorAdvancedReportingPref(&prefs2);
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_FALSE(service.last_enabled());
  EXPECT_FALSE(service.last_reset_client_state());
  service.ResetNotification();

  // Revoke consent on first profile (prefs1 becomes false).
  // The aggregated state is already false, and remains false.
  // But since consent was revoked on a previously consented profile,
  // we expect a notification with last_reset_client_state() == true.
  MetricsReportingChoiceService::SetAdvancedReportingEnabled(&prefs1, false);
  EXPECT_FALSE(service.IsAdvancedReportingEnabledForAllProfiles());
  EXPECT_TRUE(service.was_notified());
  EXPECT_FALSE(service.last_enabled());
  EXPECT_TRUE(service.last_reset_client_state());
}

}  // namespace metrics
