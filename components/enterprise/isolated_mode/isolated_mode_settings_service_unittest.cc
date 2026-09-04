// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/isolated_mode_settings_service.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_isolated_mode {

class IsolatedModeSettingsServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    // Register the kEnterpriseIsolatedModeSettings pref used by the policy.
    RegisterProfilePrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(IsolatedModeSettingsServiceTest, DoesNotReplaceIncognitoByDefault) {
  IsolatedModeSettingsService service(&pref_service_,
                                      version_info::Channel::DEV);
  EXPECT_FALSE(service.ReplacesIncognito());
}

TEST_F(IsolatedModeSettingsServiceTest,
       DoesNotReplaceIncognitoWithFeatureOnly) {
  feature_list_.InitAndEnableFeature(kEnableEnterpriseIsolatedMode);
  IsolatedModeSettingsService service(&pref_service_,
                                      version_info::Channel::DEV);
  EXPECT_FALSE(service.ReplacesIncognito());
}

TEST_F(IsolatedModeSettingsServiceTest, DoesNotReplaceIncognitoWithPolicyOnly) {
  pref_service_.SetInteger(kEnterpriseIsolatedModeSettings,
                           static_cast<int>(IsolatedModeSetting::kEnabled));
  IsolatedModeSettingsService service(&pref_service_,
                                      version_info::Channel::DEV);
  EXPECT_FALSE(service.ReplacesIncognito());
}

TEST_F(IsolatedModeSettingsServiceTest, ReplacesIncognitoWithFeatureAndPolicy) {
  feature_list_.InitAndEnableFeature(kEnableEnterpriseIsolatedMode);
  pref_service_.SetInteger(kEnterpriseIsolatedModeSettings,
                           static_cast<int>(IsolatedModeSetting::kEnabled));
  IsolatedModeSettingsService service(&pref_service_,
                                      version_info::Channel::DEV);
  EXPECT_TRUE(service.ReplacesIncognito());
}

TEST_F(IsolatedModeSettingsServiceTest,
       ReplacesIncognitoWithCommandLineSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceEnterpriseIsolatedModeReplacesIncognito);

  // Even if feature and policy are disabled, switch should enable it.
  IsolatedModeSettingsService dev_service(&pref_service_,
                                          version_info::Channel::DEV);
  EXPECT_TRUE(dev_service.ReplacesIncognito());

  // The switch doesn't work on Beta/Stable.
  IsolatedModeSettingsService beta_service(&pref_service_,
                                           version_info::Channel::BETA);
  EXPECT_FALSE(beta_service.ReplacesIncognito());
  IsolatedModeSettingsService stable_service(&pref_service_,
                                             version_info::Channel::STABLE);
  EXPECT_FALSE(stable_service.ReplacesIncognito());
}

TEST_F(IsolatedModeSettingsServiceTest, IgnoresPrefChangeAfterStartup) {
  feature_list_.InitAndEnableFeature(kEnableEnterpriseIsolatedMode);
  // Initial service evaluation when policy is not set.
  IsolatedModeSettingsService service(&pref_service_,
                                      version_info::Channel::DEV);
  EXPECT_FALSE(service.ReplacesIncognito());

  // Dynamically modifying the pref at runtime does not alter the service state.
  pref_service_.SetInteger(kEnterpriseIsolatedModeSettings,
                           static_cast<int>(IsolatedModeSetting::kEnabled));
  EXPECT_FALSE(service.ReplacesIncognito());

  // Simulating restart/new service creation picks up the new pref value.
  IsolatedModeSettingsService new_service(&pref_service_,
                                          version_info::Channel::DEV);
  EXPECT_TRUE(new_service.ReplacesIncognito());
}

TEST_F(IsolatedModeSettingsServiceTest,
       DoesNotReplaceIncognitoWithNullPrefService) {
  feature_list_.InitAndEnableFeature(kEnableEnterpriseIsolatedMode);
  IsolatedModeSettingsService service(nullptr, version_info::Channel::DEV);
  EXPECT_FALSE(service.ReplacesIncognito());
}

}  // namespace enterprise_isolated_mode
