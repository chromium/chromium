// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/settings.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_isolated_mode {

class SettingsTest : public testing::Test {
 protected:
  void SetUp() override {
    // Register the pref used by the policy.
    // Note: In components, we use
    // enterprise_isolated_mode::RegisterProfilePrefs which registers it as an
    // Integer pref.
    RegisterProfilePrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SettingsTest, DisabledByDefault) {
  EXPECT_FALSE(
      IsolatedModeReplacesIncognito(pref_service_, version_info::Channel::DEV));
}

TEST_F(SettingsTest, FeatureOnlyDoesNotEnable) {
  feature_list_.InitAndEnableFeature(kEnableEnterpriseIsolatedMode);
  EXPECT_FALSE(
      IsolatedModeReplacesIncognito(pref_service_, version_info::Channel::DEV));
}

TEST_F(SettingsTest, PolicyOnlyDoesNotEnable) {
  pref_service_.SetInteger(kEnterpriseIsolatedModeSettings, 1);
  EXPECT_FALSE(
      IsolatedModeReplacesIncognito(pref_service_, version_info::Channel::DEV));
}

TEST_F(SettingsTest, FeatureAndPolicyEnables) {
  feature_list_.InitAndEnableFeature(kEnableEnterpriseIsolatedMode);
  pref_service_.SetInteger(kEnterpriseIsolatedModeSettings, 1);
  EXPECT_TRUE(
      IsolatedModeReplacesIncognito(pref_service_, version_info::Channel::DEV));
}

TEST_F(SettingsTest, CommandLineSwitchPriority) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceEnterpriseIsolatedModeReplacesIncognito);

  // Even if feature and policy are disabled, switch should enable it.
  EXPECT_TRUE(
      IsolatedModeReplacesIncognito(pref_service_, version_info::Channel::DEV));
  // The switch doesn't work on Beta/Stable.
  EXPECT_FALSE(IsolatedModeReplacesIncognito(pref_service_,
                                             version_info::Channel::BETA));
  EXPECT_FALSE(IsolatedModeReplacesIncognito(pref_service_,
                                             version_info::Channel::STABLE));
}

}  // namespace enterprise_isolated_mode
