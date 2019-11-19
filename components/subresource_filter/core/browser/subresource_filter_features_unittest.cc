// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

class ScopedExperimentalStateToggle {
 public:
  ScopedExperimentalStateToggle(base::FeatureList::OverrideState feature_state,
                                base::FieldTrialParams variation_params)
      : scoped_configurator_(nullptr) {
    const base::Feature& kFeature = kSafeBrowsingSubresourceFilter;

    // Handle OVERRIDE_USE_DEFAULT which ScopedFeatureList does not support.
    if (feature_state == base::FeatureList::OVERRIDE_USE_DEFAULT) {
      // Init a temp ScopedFeatureList to query the current default state.
      // Note that this will take account any overrides coming from the
      // command-line, unlike testing the feature's |default_state|.
      base::test::ScopedFeatureList temp_scoped_feature_list;
      temp_scoped_feature_list.Init();
      if (base::FeatureList::IsEnabled(kFeature)) {
        feature_state = base::FeatureList::OVERRIDE_ENABLE_FEATURE;
      } else {
        feature_state = base::FeatureList::OVERRIDE_DISABLE_FEATURE;
      }
    }

    switch (feature_state) {
      case base::FeatureList::OVERRIDE_ENABLE_FEATURE:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            kFeature, variation_params);
        break;

      case base::FeatureList::OVERRIDE_DISABLE_FEATURE:
        scoped_feature_list_.InitAndDisableFeature(kFeature);
        break;

      case base::FeatureList::OVERRIDE_USE_DEFAULT:
        NOTREACHED();
        break;
    }
  }

  ~ScopedExperimentalStateToggle() {
  }

 private:
  testing::ScopedSubresourceFilterConfigurator scoped_configurator_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExperimentalStateToggle);
};

void ExpectAndRetrieveExactlyOneEnabledConfig(Configuration* actual_config) {
  DCHECK(actual_config);
  const auto config_list = GetEnabledConfigurations();
  ASSERT_EQ(1u, config_list->configs_by_decreasing_priority().size());
  *actual_config = config_list->configs_by_decreasing_priority().front();
}

void ExpectAndRetrieveExactlyOneExtraEnabledConfig(
    Configuration* actual_config) {
  DCHECK(actual_config);
  const auto config_list = GetEnabledConfigurations();
  ASSERT_EQ(4u, config_list->configs_by_decreasing_priority().size());
  *actual_config = config_list->configs_by_decreasing_priority().back();
}

void ExpectPresetCanBeEnabledByName(Configuration preset, const char* name) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      {{kEnablePresetsParameterName, name}});

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(config_list->configs_by_decreasing_priority(),
              ::testing::ElementsAre(
                  Configuration::MakePresetForLiveRunOnPhishingSites(),
                  Configuration::MakePresetForLiveRunForBetterAds(), preset,
                  Configuration()));
}

void ExpectParamsGeneratePreset(
    Configuration preset,
    std::map<std::string, std::string> variation_params) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, variation_params);

  Configuration experimental_configuration;
  const auto config_list = GetEnabledConfigurations();
  bool matched_preset = false;
  for (auto it : config_list->configs_by_decreasing_priority()) {
    matched_preset |= preset == it;
  }
  EXPECT_TRUE(matched_preset);
}

}  // namespace

class SubresourceFilterFeaturesTest : public ::testing::Test {
 public:
  SubresourceFilterFeaturesTest() {}
  ~SubresourceFilterFeaturesTest() override {}

  void SetUp() override {
    // Reset the global configuration at the start so tests start without a
    // cached value from a previous in-process test run.
    testing::GetAndSetActivateConfigurations(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterFeaturesTest);
};

TEST_F(SubresourceFilterFeaturesTest, ActivationLevel) {
  const struct {
    bool feature_enabled;
    const char* activation_level_param;
    mojom::ActivationLevel expected_activation_level;
  } kTestCases[] = {
      {false, "", mojom::ActivationLevel::kDisabled},
      {false, "disabled", mojom::ActivationLevel::kDisabled},
      {false, "dryrun", mojom::ActivationLevel::kDisabled},
      {false, "enabled", mojom::ActivationLevel::kDisabled},
      {false, "%$ garbage !%", mojom::ActivationLevel::kDisabled},
      {true, "", mojom::ActivationLevel::kDisabled},
      {true, "disable", mojom::ActivationLevel::kDisabled},
      {true, "Disable", mojom::ActivationLevel::kDisabled},
      {true, "disabled", mojom::ActivationLevel::kDisabled},
      {true, "%$ garbage !%", mojom::ActivationLevel::kDisabled},
      {true, kActivationLevelDryRun, mojom::ActivationLevel::kDryRun},
      {true, kActivationLevelEnabled, mojom::ActivationLevel::kEnabled},
      {true, "Enabled", mojom::ActivationLevel::kEnabled}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationLevelParam = \"")
                 << test_case.activation_level_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_USE_DEFAULT
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kActivationLevelParameterName, test_case.activation_level_param},
         {kActivationScopeParameterName, kActivationScopeNoSites}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(test_case.expected_activation_level,
              actual_configuration.activation_options.activation_level);
    EXPECT_EQ(ActivationScope::NO_SITES,
              actual_configuration.activation_conditions.activation_scope);
  }
}

TEST_F(SubresourceFilterFeaturesTest, ActivationScope) {
  const struct {
    bool feature_enabled;
    const char* activation_scope_param;
    ActivationScope expected_activation_scope;
  } kTestCases[] = {
      {false, "", ActivationScope::NO_SITES},
      {false, "no_sites", ActivationScope::NO_SITES},
      {false, "allsites", ActivationScope::NO_SITES},
      {false, "enabled", ActivationScope::NO_SITES},
      {false, "%$ garbage !%", ActivationScope::NO_SITES},
      {true, "", ActivationScope::NO_SITES},
      {true, "nosites", ActivationScope::NO_SITES},
      {true, "No_sites", ActivationScope::NO_SITES},
      {true, "no_sites", ActivationScope::NO_SITES},
      {true, "%$ garbage !%", ActivationScope::NO_SITES},
      {true, kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationScopeParam = \"")
                 << test_case.activation_scope_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kActivationLevelParameterName, kActivationLevelDisabled},
         {kActivationScopeParameterName, test_case.activation_scope_param}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(mojom::ActivationLevel::kDisabled,
              actual_configuration.activation_options.activation_level);
    EXPECT_EQ(test_case.expected_activation_scope,
              actual_configuration.activation_conditions.activation_scope);
  }
}

TEST_F(SubresourceFilterFeaturesTest, ActivationLevelAndScope) {
  const struct {
    bool feature_enabled;
    const char* activation_level_param;
    mojom::ActivationLevel expected_activation_level;
    const char* activation_scope_param;
    ActivationScope expected_activation_scope;
  } kTestCases[] = {
      {false, kActivationLevelDisabled, mojom::ActivationLevel::kDisabled,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelDisabled, mojom::ActivationLevel::kDisabled,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelDisabled, mojom::ActivationLevel::kDisabled,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelDisabled, mojom::ActivationLevel::kDisabled,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationLevelDisabled, mojom::ActivationLevel::kDisabled,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelDryRun, mojom::ActivationLevel::kDryRun,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelDryRun, mojom::ActivationLevel::kDryRun,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelDryRun, mojom::ActivationLevel::kDryRun,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationLevelDryRun, mojom::ActivationLevel::kDryRun,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelEnabled, mojom::ActivationLevel::kEnabled,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelEnabled, mojom::ActivationLevel::kEnabled,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelEnabled, mojom::ActivationLevel::kEnabled,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationLevelEnabled, mojom::ActivationLevel::kEnabled,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {false, kActivationLevelEnabled, mojom::ActivationLevel::kDisabled,
       kActivationScopeAllSites, ActivationScope::NO_SITES}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationLevelParam = \"")
                 << test_case.activation_level_param << "\"");
    SCOPED_TRACE(::testing::Message("ActivationScopeParam = \"")
                 << test_case.activation_scope_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_USE_DEFAULT
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kActivationLevelParameterName, test_case.activation_level_param},
         {kActivationScopeParameterName, test_case.activation_scope_param}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(test_case.expected_activation_level,
              actual_configuration.activation_options.activation_level);
    EXPECT_EQ(test_case.expected_activation_scope,
              actual_configuration.activation_conditions.activation_scope);
  }
}

TEST_F(SubresourceFilterFeaturesTest, ActivationList) {
  const std::string activation_soc_eng(
      kActivationListSocialEngineeringAdsInterstitial);
  const std::string activation_phishing(kActivationListPhishingInterstitial);
  const std::string socEngPhising = activation_soc_eng + activation_phishing;
  const std::string socEngCommaPhising =
      activation_soc_eng + "," + activation_phishing;
  const std::string phishingCommaSocEng =
      activation_phishing + "," + activation_soc_eng;
  const std::string socEngCommaPhisingCommaGarbage =
      socEngCommaPhising + "," + "Garbage";
  const struct {
    bool feature_enabled;
    const char* activation_list_param;
    ActivationList expected_activation_list;
  } kTestCases[] = {
      {false, "", ActivationList::NONE},
      {false, "social eng ads intertitial", ActivationList::NONE},
      {false, "phishing,interstitial", ActivationList::NONE},
      {false, "%$ garbage !%", ActivationList::NONE},
      {true, "", ActivationList::NONE},
      {true, "social eng ads intertitial", ActivationList::NONE},
      {true, "phishing interstitial", ActivationList::NONE},
      {true, "%$ garbage !%", ActivationList::NONE},
      {true, kActivationListSocialEngineeringAdsInterstitial,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL},
      {true, kActivationListBetterAds, ActivationList::BETTER_ADS},
      {true, kActivationListPhishingInterstitial,
       ActivationList::PHISHING_INTERSTITIAL},
      {true, socEngPhising.c_str(), ActivationList::NONE},
      {true, socEngCommaPhising.c_str(), ActivationList::PHISHING_INTERSTITIAL},
      {true, phishingCommaSocEng.c_str(),
       ActivationList::PHISHING_INTERSTITIAL},
      {true, socEngCommaPhisingCommaGarbage.c_str(),
       ActivationList::PHISHING_INTERSTITIAL},
      {true, "List1, List2", ActivationList::NONE}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationListParam = \"")
                 << test_case.activation_list_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_USE_DEFAULT
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kActivationLevelParameterName, kActivationLevelDisabled},
         {kActivationScopeParameterName, kActivationScopeNoSites},
         {kActivationListsParameterName, test_case.activation_list_param}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(test_case.expected_activation_list,
              actual_configuration.activation_conditions.activation_list);
  }
}

TEST_F(SubresourceFilterFeaturesTest, ActivationPriority) {
  const struct {
    bool feature_enabled;
    const char* activation_priority_param;
    int expected_priority;
  } kTestCases[] = {{false, "", 0},
                    {false, "not_an_integer", 0},
                    {false, "100", 0},
                    {true, "", 0},
                    {true, "not_an_integer", 0},
                    {true, "0.5not_an_integer", 0},
                    {true, "garbage42", 0},
                    {true, "42garbage", 42},
                    {true, "0", 0},
                    {true, "1", 1},
                    {true, "-1", -1},
                    {true, "2.9", 2},
                    {true, "-2.9", -2},
                    {true, "2e0", 2},
                    {true, "100", 100}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("Priority = \"")
                 << test_case.activation_priority_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_USE_DEFAULT
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kActivationPriorityParameterName,
          test_case.activation_priority_param}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(test_case.expected_priority,
              actual_configuration.activation_conditions.priority);
  }
}

TEST_F(SubresourceFilterFeaturesTest, PerfMeasurementRate) {
  const struct {
    bool feature_enabled;
    const char* perf_measurement_param;
    double expected_perf_measurement_rate;
  } kTestCases[] = {{false, "not_a_number", 0},
                    {false, "0", 0},
                    {false, "1", 0},
                    {true, "not_a_number", 0},
                    {true, "0.5not_a_number", 0},
                    {true, "0", 0},
                    {true, "0.000", 0},
                    {true, "0.05", 0.05},
                    {true, "0.5", 0.5},
                    {true, "1", 1},
                    {true, "1.0", 1},
                    {true, "0.333", 0.333},
                    {true, "1e0", 1},
                    {true, "5", 1}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("PerfMeasurementParam = \"")
                 << test_case.perf_measurement_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_USE_DEFAULT
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kPerformanceMeasurementRateParameterName,
          test_case.perf_measurement_param}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(
        test_case.expected_perf_measurement_rate,
        actual_configuration.activation_options.performance_measurement_rate);
  }
}

TEST_F(SubresourceFilterFeaturesTest, RulesetFlavor) {
  const struct {
    bool feature_enabled;
    const char* ruleset_flavor_param;
    const char* expected_ruleset_flavor_value;
  } kTestCases[] = {
      {false, "", ""}, {false, "a", ""}, {false, "test value", ""},
      {true, "", ""},  {true, "a", "a"}, {true, "test value", "test value"}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("Flavor = \"")
                 << test_case.ruleset_flavor_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_USE_DEFAULT
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        {{kRulesetFlavorParameterName, test_case.ruleset_flavor_param}});

    Configuration actual_configuration;
    if (test_case.feature_enabled) {
      ExpectAndRetrieveExactlyOneExtraEnabledConfig(&actual_configuration);
    } else {
      ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    }
    EXPECT_EQ(std::string(test_case.expected_ruleset_flavor_value),
              actual_configuration.general_settings.ruleset_flavor);
  }
}

TEST_F(SubresourceFilterFeaturesTest, LexicographicallyGreatestRulesetFlavor) {
  const struct {
    const char* expected_ruleset_flavor_selected;
    std::vector<std::string> ruleset_flavors;
  } kTestCases[] = {{"", std::vector<std::string>()},
                    {"", {""}},
                    {"a", {"a"}},
                    {"e", {"e"}},
                    {"foo", {"foo"}},
                    {"", {"", ""}},
                    {"a", {"a", ""}},
                    {"a", {"", "a"}},
                    {"a", {"a", "a"}},
                    {"c", {"b", "", "c"}},
                    {"b", {"", "b", "a"}},
                    {"aa", {"", "a", "aa"}},
                    {"b", {"", "a", "aa", "b"}},
                    {"foo", {"foo", "bar", "b", ""}},
                    {"2.1", {"2", "2.1", "1.3", ""}},
                    {"3", {"2", "2.1", "1.3", "3"}}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "ruleset_flavors: "
                 << ::testing::PrintToString(test_case.ruleset_flavors));

    std::vector<Configuration> configs;
    for (const auto& ruleset_flavor : test_case.ruleset_flavors) {
      Configuration config;
      config.general_settings.ruleset_flavor = ruleset_flavor;
      configs.push_back(std::move(config));
    }

    subresource_filter::testing::ScopedSubresourceFilterConfigurator
        scoped_configuration(std::move(configs));
    EXPECT_EQ(test_case.expected_ruleset_flavor_selected,
              GetEnabledConfigurations()
                  ->lexicographically_greatest_ruleset_flavor());
  }
}

TEST_F(SubresourceFilterFeaturesTest, EnabledConfigurations_FeatureDisabled) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      std::map<std::string, std::string>());

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(config_list->configs_by_decreasing_priority(),
              ::testing::ElementsAre(Configuration()));
  EXPECT_EQ(std::string(),
            config_list->lexicographically_greatest_ruleset_flavor());
}

TEST_F(SubresourceFilterFeaturesTest,
       EnabledConfigurations_FeatureEnabledWithNoParameters) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      std::map<std::string, std::string>());

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(
      config_list->configs_by_decreasing_priority(),
      ::testing::ElementsAre(
          Configuration::MakePresetForLiveRunOnPhishingSites(),
          Configuration::MakePresetForLiveRunForBetterAds(),
          Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
          Configuration()));
  EXPECT_EQ(std::string(),
            config_list->lexicographically_greatest_ruleset_flavor());
}

TEST_F(SubresourceFilterFeaturesTest,
       PresetForPerformanceTestingDryRunOnAllSites) {
  ExpectPresetCanBeEnabledByName(
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      kPresetPerformanceTestingDryRunOnAllSites);
  ExpectParamsGeneratePreset(
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      {{kActivationLevelParameterName, kActivationLevelDryRun},
       {kActivationScopeParameterName, kActivationScopeAllSites},
       {kActivationPriorityParameterName, "500"},
       {kPerformanceMeasurementRateParameterName, "0.01"}});
}

TEST_F(SubresourceFilterFeaturesTest, PresetForLiveRunOnBetterAdsSites) {
  const Configuration config =
      Configuration::MakePresetForLiveRunForBetterAds();
  EXPECT_EQ(ActivationList::BETTER_ADS,
            config.activation_conditions.activation_list);
  EXPECT_EQ(ActivationScope::ACTIVATION_LIST,
            config.activation_conditions.activation_scope);
  EXPECT_EQ(800, config.activation_conditions.priority);
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            config.activation_options.activation_level);
  EXPECT_EQ(0.0, config.activation_options.performance_measurement_rate);
}

TEST_F(SubresourceFilterFeaturesTest, ConfigurationPriorities) {
  const std::vector<Configuration> expected_order_by_decreasing_priority = {
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      Configuration::MakePresetForLiveRunForBetterAds(),
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      Configuration() /* default constructor */
  };

  std::vector<Configuration> shuffled_order = {
      expected_order_by_decreasing_priority[2],
      expected_order_by_decreasing_priority[3],
      expected_order_by_decreasing_priority[0],
      expected_order_by_decreasing_priority[1]};
  subresource_filter::testing::ScopedSubresourceFilterConfigurator
      scoped_configuration(std::move(shuffled_order));
  EXPECT_THAT(
      GetEnabledConfigurations()->configs_by_decreasing_priority(),
      ::testing::ElementsAreArray(expected_order_by_decreasing_priority));
}

TEST_F(SubresourceFilterFeaturesTest, EnableDisableMultiplePresets) {
  const std::string kPhishing(kPresetLiveRunOnPhishingSites);
  const std::string kPerfTest(kPresetPerformanceTestingDryRunOnAllSites);
  const std::string kBAS(kPresetLiveRunForBetterAds);

  // The default config comes from the empty experimental configuration.
  const std::vector<Configuration> kEmptyConfig = {Configuration()};
  const std::vector<Configuration> kSmallConfig = {
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      Configuration::MakePresetForLiveRunForBetterAds(), Configuration()};
  const std::vector<Configuration> kDefaultConfig = {
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      Configuration::MakePresetForLiveRunForBetterAds(),
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      Configuration()};

  const struct {
    std::string enable_preset_name_list;
    std::string disable_preset_name_list;
    const std::vector<Configuration> expected_configs;
  } kTestCases[] = {
      {"", "", kDefaultConfig},
      {"garbage1", "garbage2", kDefaultConfig},
      {"", kPhishing + "," + kPerfTest + "," + kBAS, kEmptyConfig},
      {kPhishing, kPerfTest, kSmallConfig},
      {kPerfTest, "garbage", kDefaultConfig},
      {kPerfTest, base::ToUpperASCII(kPerfTest), kSmallConfig},
      {kPerfTest + "," + kPhishing + "," + kBAS,
       ",,garbage, ," + kPerfTest + "," + kPhishing + "," + kBAS, kEmptyConfig},
      {base::ToUpperASCII(kPhishing) + "," + base::ToUpperASCII(kPerfTest), "",
       kDefaultConfig},
      {",, ," + kPerfTest + ",," + kPhishing, "", kDefaultConfig},
      {"garbage,garbage2," + kPerfTest + "," + kPhishing, "", kDefaultConfig}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        ::testing::Message()
        << "enable_preset_name_list: " << test_case.enable_preset_name_list
        << " disable_preset_name_list: " << test_case.disable_preset_name_list);

    ScopedExperimentalStateToggle scoped_experimental_state(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        {{kEnablePresetsParameterName, test_case.enable_preset_name_list},
         {kDisablePresetsParameterName, test_case.disable_preset_name_list}});

    const auto config_list = GetEnabledConfigurations();
    EXPECT_THAT(config_list->configs_by_decreasing_priority(),
                ::testing::ElementsAreArray(test_case.expected_configs));
    EXPECT_EQ(std::string(),
              config_list->lexicographically_greatest_ruleset_flavor());
  }
}

TEST_F(SubresourceFilterFeaturesTest,
       EnableMultiplePresetsAndExperimentalConfig) {
  const std::string kPhishing(kPresetLiveRunOnPhishingSites);
  const std::string kPerfTest(kPresetPerformanceTestingDryRunOnAllSites);
  const std::string kBAS(kPresetLiveRunForBetterAds);
  const std::string kTestRulesetFlavor("foobar");

  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      {{kEnablePresetsParameterName, kPhishing + "," + kPerfTest + "," + kBAS},
       {kActivationLevelParameterName, kActivationLevelDryRun},
       {kActivationScopeParameterName, kActivationScopeActivationList},
       {kActivationListsParameterName, kActivationListSubresourceFilter},
       {kActivationPriorityParameterName, "750"},
       {kRulesetFlavorParameterName, kTestRulesetFlavor}});

  Configuration experimental_config(mojom::ActivationLevel::kDryRun,
                                    ActivationScope::ACTIVATION_LIST,
                                    ActivationList::SUBRESOURCE_FILTER);
  experimental_config.activation_conditions.priority = 750;
  experimental_config.general_settings.ruleset_flavor = kTestRulesetFlavor;

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(
      config_list->configs_by_decreasing_priority(),
      ::testing::ElementsAre(
          Configuration::MakePresetForLiveRunOnPhishingSites(),
          Configuration::MakePresetForLiveRunForBetterAds(),
          experimental_config,
          Configuration::MakePresetForPerformanceTestingDryRunOnAllSites()));
  EXPECT_EQ(kTestRulesetFlavor,
            config_list->lexicographically_greatest_ruleset_flavor());
}

TEST_F(SubresourceFilterFeaturesTest, AdTagging_EnablesDryRun) {
  const Configuration dryrun =
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites();
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(kAdTagging);
  EXPECT_TRUE(base::Contains(
      GetEnabledConfigurations()->configs_by_decreasing_priority(), dryrun));
}

TEST_F(SubresourceFilterFeaturesTest, AdTaggingDisabled_DisablesDryRun) {
  const Configuration dryrun =
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites();
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(kAdTagging);
  EXPECT_FALSE(base::Contains(
      GetEnabledConfigurations()->configs_by_decreasing_priority(), dryrun));
}

}  // namespace subresource_filter
