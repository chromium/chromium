// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/startup_util.h"

#include <array>
#include <memory>
#include <ostream>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

// Note: this operator must be defined in the same namespace as the type it is
// intended for, i.e., StartupMode.
std::ostream& operator<<(std::ostream& out, const StartupMode& result) {
  switch (result) {
    case StartupMode::FEATURE_DISABLED:
      out << "FEATURE_DISABLED";
      break;
    case StartupMode::MANDATORY_PARAMETERS_MISSING:
      out << "MANDATORY_PARAMETERS_MISSING";
      break;
    case StartupMode::SETTING_DISABLED:
      out << "SETTING_DISABLED";
      break;
    case StartupMode::NO_INITIAL_URL:
      out << "NO_INITIAL_URL";
      break;
    case StartupMode::START_REGULAR:
      out << "START_REGULAR";
      break;
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
      out << "START_RPC_TRIGGER_SCRIPT";
      break;
  }
  return out;
}

namespace {

using features::kAutofillAssistant;
using features::kAutofillAssistantChromeEntry;
using features::kAutofillAssistantGetTriggerScriptsByHashPrefix;
using features::kAutofillAssistantLoadDFMForTriggerScripts;
using features::kAutofillAssistantProactiveHelp;
using ::testing::Eq;
using ::testing::ValuesIn;

// Feature configurations to instantiate tests with.
struct TestFeatureConfig {
  std::vector<base::Feature> enabled_features;
};

// Shorthand for the full set of relevant features.
const std::array<base::Feature, 5> kFullFeatureSet = {
    kAutofillAssistant, kAutofillAssistantProactiveHelp,
    kAutofillAssistantChromeEntry, kAutofillAssistantLoadDFMForTriggerScripts,
    kAutofillAssistantGetTriggerScriptsByHashPrefix};

// Common script parameters to reuse.
const base::flat_map<std::string, std::string> kRegularScript = {
    {"ENABLED", "true"},
    {"START_IMMEDIATELY", "true"},
    {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
const base::flat_map<std::string, std::string> kRequestTriggerScript = {
    {"ENABLED", "true"},
    {"START_IMMEDIATELY", "false"},
    {"REQUEST_TRIGGER_SCRIPT", "true"},
    {"ORIGINAL_DEEPLINK", "https://www.example.com"}};

const TriggerContext::Options kDefaultCCTOptions = {
    std::string(), /* is_cct = */ true,
    false,         false,
    std::string(), false,
    false,         false,
    true};

const TriggerContext::Options kDefaultNonCCTOptions = {
    std::string(), /* is_cct = */ false,
    false,         false,
    std::string(), false,
    false,         false,
    true};

// The set of feature combinations to test.
const TestFeatureConfig kTestFeatureConfigs[] = {
    // All features are disabled.
    {{}},
    // Only AutofillAssistant is enabled.
    {{kAutofillAssistant}},
    // AutofillAssistant and ChromeEntry, but not ProactiveHelp.
    {{kAutofillAssistant, kAutofillAssistantChromeEntry}},
    // Everything except LoadDFMForTriggerScripts.
    {{kAutofillAssistant, kAutofillAssistantChromeEntry,
      kAutofillAssistantProactiveHelp}},
    // All features are enabled.
    {{kFullFeatureSet.begin(), kFullFeatureSet.end()}}};

// Custom output operator overloads to provide human-readable test outputs.
std::ostream& operator<<(std::ostream& out, const base::Feature& feature) {
  out << feature.name;
  return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& vec) {
  out << "[";
  std::string separator;
  for (const auto& item : vec) {
    out << separator << item;
    separator.assign(",");
  }
  out << "]";
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestFeatureConfig& config) {
  out << "enabled_features=" << config.enabled_features << ", ";
  return out;
}

std::string ToString(const StartupMode& result) {
  std::ostringstream stream;
  stream << result;
  return stream.str();
}

// Custom matcher for |StartupMode| with human-readable output.
MATCHER_P(MatchingStartupMode,
          expected_result,
          ToString(StartupMode{expected_result})) {
  return arg == expected_result;
}

// Regular test fixture for non-parametrized tests.
class StartupUtilTest : public testing::Test {};

// Parametrized test fixture for tests that should be run against a variety of
// different feature configurations.
class StartupUtilParametrizedTest
    : public StartupUtilTest,
      public testing::WithParamInterface<TestFeatureConfig> {
 public:
  void SetUp() override {
    StartupUtilTest::SetUp();
    std::vector<base::Feature> disabled_features;
    for (const auto& feature : kFullFeatureSet) {
      if (!IsFeatureEnabled(feature)) {
        disabled_features.emplace_back(feature);
      }
    }

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(
        /* enabled_features = */ GetParam().enabled_features,
        /* disabled_features = */ disabled_features);
  }

  void TearDown() override { scoped_feature_list_.reset(); }

  bool IsAnyFeatureSetEnabled(
      const std::vector<std::vector<base::Feature>>& feature_sets) {
    for (const auto& features : feature_sets) {
      if (AreFeaturesEnabled(features)) {
        return true;
      }
    }
    return false;
  }

  bool AreFeaturesEnabled(const std::vector<base::Feature>& features) const {
    for (const auto& feature : features) {
      if (!IsFeatureEnabled(feature)) {
        return false;
      }
    }
    return true;
  }

  // Returns whether |feature| is enabled for the current run.
  bool IsFeatureEnabled(const base::Feature& feature) const {
    return base::Contains(GetParam().enabled_features, feature.name,
                          &base::Feature::name);
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_P(StartupUtilParametrizedTest, StartRegularScript) {
  // CCT, DFM installation required.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{std::make_unique<ScriptParameters>(kRegularScript),
                         kDefaultCCTOptions},
          {.feature_module_installed = false}),
      MatchingStartupMode(IsFeatureEnabled(kAutofillAssistant)
                              ? StartupMode::START_REGULAR
                              : StartupMode::FEATURE_DISABLED));

  // Regular tab, DFM installation required.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{std::make_unique<ScriptParameters>(kRegularScript),
                         kDefaultNonCCTOptions},
          {.feature_module_installed = false}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantChromeEntry})
                              ? StartupMode::START_REGULAR
                              : StartupMode::FEATURE_DISABLED));

  // Regular tab, DFM already installed.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{std::make_unique<ScriptParameters>(kRegularScript),
                         kDefaultNonCCTOptions},
          {.feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantChromeEntry})
                              ? StartupMode::START_REGULAR
                              : StartupMode::FEATURE_DISABLED));
}

TEST_P(StartupUtilParametrizedTest, StartRpcTriggerScript) {
  // Everything true, DFM already installed.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(kRequestTriggerScript),
              kDefaultCCTOptions},
          {.msbb_setting_enabled = true,
           .proactive_help_setting_enabled = true,
           .feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantProactiveHelp})
                              ? StartupMode::START_RPC_TRIGGER_SCRIPT
                              : StartupMode::FEATURE_DISABLED));

  // Everything true, but DFM is not yet installed.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(kRequestTriggerScript),
              kDefaultNonCCTOptions},
          {.msbb_setting_enabled = true,
           .proactive_help_setting_enabled = true,
           .feature_module_installed = false}),
      MatchingStartupMode(
          AreFeaturesEnabled({kAutofillAssistant, kAutofillAssistantChromeEntry,
                              kAutofillAssistantLoadDFMForTriggerScripts})
              ? StartupMode::START_RPC_TRIGGER_SCRIPT
              : StartupMode::FEATURE_DISABLED));

  // CCT, MSBB is off, but kAutofillAssistantGetTriggerScriptsByHashPrefix might
  // be enabled.
  StartupMode expectedStartupMode =
      IsFeatureEnabled(kAutofillAssistantGetTriggerScriptsByHashPrefix)
          ? StartupMode::START_RPC_TRIGGER_SCRIPT
          : StartupMode::SETTING_DISABLED;
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(kRequestTriggerScript),
              kDefaultCCTOptions},
          {.msbb_setting_enabled = false,
           .proactive_help_setting_enabled = true,
           .feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantProactiveHelp})
                              ? expectedStartupMode
                              : StartupMode::FEATURE_DISABLED));

  // CCT, Proactive help is off.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(kRequestTriggerScript),
              kDefaultCCTOptions},
          {.msbb_setting_enabled = true,
           .proactive_help_setting_enabled = false,
           .feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantProactiveHelp})
                              ? StartupMode::SETTING_DISABLED
                              : StartupMode::FEATURE_DISABLED));
}

TEST_P(StartupUtilParametrizedTest, InvalidParameterCombinationsShouldFail) {
  // START_IMMEDIATELY=false requires REQUEST_TRIGGER_SCRIPT
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(
                  base::flat_map<std::string, std::string>{
                      {"ENABLED", "true"},
                      {"START_IMMEDIATELY", "false"},
                      {"ORIGINAL_DEEPLINK", "https://www.example.com"}}),
              kDefaultCCTOptions},
          {.msbb_setting_enabled = true,
           .proactive_help_setting_enabled = false,
           .feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantProactiveHelp})
                              ? StartupMode::MANDATORY_PARAMETERS_MISSING
                              : StartupMode::FEATURE_DISABLED));

  // REQUEST_TRIGGER_SCRIPT must not only be specified, but set to true.
  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(
                  base::flat_map<std::string, std::string>{
                      {"ENABLED", "true"},
                      {"START_IMMEDIATELY", "false"},
                      {"REQUEST_TRIGGER_SCRIPT", "false"},
                      {"ORIGINAL_DEEPLINK", "https://www.example.com"}}),
              kDefaultCCTOptions},
          {.msbb_setting_enabled = true,
           .proactive_help_setting_enabled = false,
           .feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant,
                                              kAutofillAssistantProactiveHelp})
                              ? StartupMode::MANDATORY_PARAMETERS_MISSING
                              : StartupMode::FEATURE_DISABLED));

  // ORIGINAL_DEEPLINK or initial url must be specified and valid.
  EXPECT_THAT(StartupUtil().ChooseStartupModeForIntent(
                  TriggerContext{std::make_unique<ScriptParameters>(
                                     base::flat_map<std::string, std::string>{
                                         {"ENABLED", "true"},
                                         {"START_IMMEDIATELY", "true"}}),
                                 kDefaultCCTOptions},
                  {.msbb_setting_enabled = true,
                   .proactive_help_setting_enabled = true,
                   .feature_module_installed = true}),
              MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant})
                                      ? StartupMode::NO_INITIAL_URL
                                      : StartupMode::FEATURE_DISABLED));

  EXPECT_THAT(
      StartupUtil().ChooseStartupModeForIntent(
          TriggerContext{
              std::make_unique<ScriptParameters>(
                  base::flat_map<std::string, std::string>{
                      {"ENABLED", "true"}, {"START_IMMEDIATELY", "true"}}),
              {std::string(), /* is_cct = */ true, false, false,
               /* initial_url = */ "https://www.example.com", false, false,
               false, true}},
          {.msbb_setting_enabled = true,
           .proactive_help_setting_enabled = true,
           .feature_module_installed = true}),
      MatchingStartupMode(AreFeaturesEnabled({kAutofillAssistant})
                              ? StartupMode::START_REGULAR
                              : StartupMode::FEATURE_DISABLED));
}

INSTANTIATE_TEST_SUITE_P(StartupParamTestSuite,
                         StartupUtilParametrizedTest,
                         ValuesIn(kTestFeatureConfigs));

TEST_F(StartupUtilTest, ChooseStartupUrlForIntentPrefersOriginalDeeplink) {
  base::flat_map<std::string, std::string> script_parameters = {
      {"ORIGINAL_DEEPLINK", "https://www.original-deeplink.com"}};

  EXPECT_THAT(StartupUtil().ChooseStartupUrlForIntent(
                  {std::make_unique<ScriptParameters>(script_parameters),
                   TriggerContext::Options{}}),
              Eq(GURL("https://www.original-deeplink.com")));

  TriggerContext::Options options;
  options.initial_url = "https://www.initial-url.com";
  EXPECT_THAT(
      StartupUtil().ChooseStartupUrlForIntent(
          {std::make_unique<ScriptParameters>(script_parameters), options}),
      Eq(GURL("https://www.original-deeplink.com")));
}

TEST_F(StartupUtilTest, ChooseStartupUrlForIntentFallsBackToInitialUrl) {
  TriggerContext::Options options;
  options.initial_url = "https://www.initial-url.com";
  EXPECT_THAT(StartupUtil().ChooseStartupUrlForIntent(
                  {std::make_unique<ScriptParameters>(), options}),
              Eq(GURL("https://www.initial-url.com")));
}

TEST_F(StartupUtilTest, ChooseStartupUrlForIntentFailsIfNotSpecified) {
  EXPECT_THAT(
      StartupUtil().ChooseStartupUrlForIntent(
          {std::make_unique<ScriptParameters>(), TriggerContext::Options{}}),
      Eq(absl::nullopt));
}

}  // namespace
}  // namespace autofill_assistant
