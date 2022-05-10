// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PerformanceManagerMetricsProviderTest : public testing::Test {
 protected:
  PrefService* local_state() { return &local_state_; }

  void SetHighEfficiencyEnabled(bool enabled) {
    local_state()->SetBoolean(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
        enabled);
  }

  void SetBatterySaverEnabled(bool enabled) {
    local_state()->SetBoolean(
        performance_manager::user_tuning::prefs::kBatterySaverModeEnabled,
        enabled);
  }

  void ExpectSingleUniqueSample(
      const base::HistogramTester& tester,
      performance_manager::MetricsProvider::EfficiencyMode sample) {
    tester.ExpectUniqueSample("PerformanceManager.UserTuning.EfficiencyMode",
                              sample, 1);
  }

 private:
  void SetUp() override {
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  TestingPrefServiceSimple local_state_;
};

TEST_F(PerformanceManagerMetricsProviderTest, TestNormalMode) {
  base::HistogramTester tester;

  performance_manager::MetricsProvider provider(local_state());
  provider.ProvideCurrentSessionData(nullptr);

  ExpectSingleUniqueSample(
      tester, performance_manager::MetricsProvider::EfficiencyMode::kNormal);
}

TEST_F(PerformanceManagerMetricsProviderTest, TestMixedMode) {
  performance_manager::MetricsProvider provider(local_state());

  {
    base::HistogramTester tester;
    // Start in normal mode
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kNormal);
  }

  {
    base::HistogramTester tester;
    // Enabled High-Efficiency Mode, the next reported value should be "mixed"
    // because we transitioned from normal to High-Efficiency during the
    // interval.
    SetHighEfficiencyEnabled(true);
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kMixed);
  }

  {
    base::HistogramTester tester;
    // If another UMA upload happens without mode changes, this one will report
    // High-Efficiency Mode.
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProvider::EfficiencyMode::kHighEfficiency);
  }
}

TEST_F(PerformanceManagerMetricsProviderTest, TestBothModes) {
  SetHighEfficiencyEnabled(true);
  SetBatterySaverEnabled(true);

  performance_manager::MetricsProvider provider(local_state());

  {
    base::HistogramTester tester;
    // Start with both modes enabled (such as a Chrome startup after having
    // enabled both modes in a previous session).
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kBoth);
  }

  {
    base::HistogramTester tester;
    // Disabling High-Efficiency Mode will cause the next report to be "mixed".
    SetHighEfficiencyEnabled(false);
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kMixed);
  }

  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProvider::EfficiencyMode::kBatterySaver);
  }

  {
    base::HistogramTester tester;
    // Re-enabling High-Efficiency Mode will cause the next report to indicate
    // "mixed".
    SetHighEfficiencyEnabled(true);
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kMixed);
  }

  {
    base::HistogramTester tester;
    // One more report with no changes, this one reports "both" again.
    provider.ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kBoth);
  }
}