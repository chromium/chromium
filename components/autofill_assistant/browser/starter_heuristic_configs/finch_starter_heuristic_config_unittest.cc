// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/starter_heuristic_configs_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;
using ::testing::ValuesIn;

class FinchStarterHeuristicConfigTest : public testing::Test {
 public:
  FinchStarterHeuristicConfigTest() = default;
  ~FinchStarterHeuristicConfigTest() override = default;

 protected:
  // Convenience for tests where the contents of the heuristic don't matter.
  void InitDefaultHeuristic(const base::Feature& feature,
                            const std::string& parameter_key) {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeaturesAndParameters(
        {{feature, {{parameter_key, R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com"],
          "enabledInCustomTabs":true,
          "enabledInRegularTabs":true,
          "enabledInWeblayer": true,
          "enabledForSignedOutUsers": true,
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}}},
        /* disabled_features = */ {});
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(std::make_unique<FakeCommonDependencies>(
          /*identity_manager=*/nullptr));

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(FinchStarterHeuristicConfigTest, SmokeTest) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");

  FinchStarterHeuristicConfig config_enabled(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});
  EXPECT_THAT(config_enabled.GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              SizeIs(1));

  // UrlHeuristic2 was not enabled, so this should return the empty list.
  FinchStarterHeuristicConfig config_default_disabled(
      base::FeatureParam<std::string>{
          &features::kAutofillAssistantUrlHeuristic2, "some_key", ""});
  EXPECT_THAT(config_default_disabled.GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, DefaultHeuristicParsedCorrectly) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");
  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  EXPECT_THAT(config.GetIntent(), Eq("FAKE_INTENT"));
  EXPECT_THAT(
      config.GetDenylistedDomains(),
      UnorderedElementsAreArray(std::vector<std::string>{"example.com"}));
  EXPECT_EQ(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                  &context_),
            base::JSONReader::Read(R"([
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ])")
                ->GetList());
}

TEST_F(FinchStarterHeuristicConfigTest, DisabledForSupervisedUsers) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");
  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_supervised_user_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.is_supervised_user_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(FinchStarterHeuristicConfigTest,
       DisabledForNotAllowedForMachineLearningUsers) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");
  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_allowed_for_machine_learning_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.is_allowed_for_machine_learning_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(FinchStarterHeuristicConfigTest, DisabledIfProactiveHelpSettingOff) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");
  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.proactive_help_enabled_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.proactive_help_enabled_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(FinchStarterHeuristicConfigTest, FlagsDefaultToFalse) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com"],
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, DenylistDefaultsToEmpty) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ],
          "enabledInCustomTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(FinchStarterHeuristicConfigTest, InvalidDenylistBreaksConfig) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com", -1],
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ],
          "enabledInCustomTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, EmptyFieldTrialParamDoesNothing) {
  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "does_not_exist", ""});
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, InvalidFieldTrialParamNotValidJson) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          invalid json
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, InvalidFieldTrialParamNotADict) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        ["not a dictionary"]
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, InvalidFieldTrialParamNoIntent) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ],
          "enabledInCustomTabs":true,
          "enabledInRegularTabs":true,
          "enabledInWeblayer":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, MultipleConditionSets) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com"],
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            },
            {
              "conditionSet":{
                "urlContains":"different"
              }
            }
          ],
          "enabledInCustomTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(2));

  fake_platform_delegate_.is_custom_tab_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

class FinchStarterHeuristicConfigParametrizedTest
    : public FinchStarterHeuristicConfigTest,
      public testing::WithParamInterface<
          starter_heuristic_configs_test_util::ClientState> {
 public:
  void SetUp() override {
    FinchStarterHeuristicConfigTest::SetUp();
    starter_heuristic_configs_test_util::ApplyClientState(
        &fake_platform_delegate_, GetParam());
  }
};

TEST_P(FinchStarterHeuristicConfigParametrizedTest, CustomTabHeuristic) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com"],
          "enabledInCustomTabs":true,
          "enabledForSignedOutUsers":true,
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  // Tests a typical custom tab heuristic:
  // - Must not be a supervised user
  // - Must be a CCT created by GSA
  // - Proactive help and MSBB must be turned on
  bool expected_result =
      !GetParam().is_supervised_user && GetParam().is_custom_tab &&
      GetParam().is_tab_created_by_gsa && GetParam().proactive_help_enabled &&
      GetParam().msbb_enabled;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(expected_result ? 1 : 0));
}

TEST_P(FinchStarterHeuristicConfigParametrizedTest, RegularTabHeuristic) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com"],
          "enabledInCustomTabs":false,
          "enabledInRegularTabs":true,
          "enabledForSignedOutUsers":true,
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  // - Must not be a supervised user
  // - Must be a regular tab, not a weblayer
  // - Proactive help and MSBB must be turned on
  bool expected_result = !GetParam().is_supervised_user &&
                         !GetParam().is_custom_tab && !GetParam().is_weblayer &&
                         GetParam().proactive_help_enabled &&
                         GetParam().msbb_enabled;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(expected_result ? 1 : 0));
}

TEST_P(FinchStarterHeuristicConfigParametrizedTest, MostLenientHeuristic) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"some_key", R"(
        {
          "intent":"FAKE_INTENT",
          "denylistedDomains":["example.com"],
          "enabledInCustomTabs":true,
          "enabledInRegularTabs":true,
          "enabledInWeblayer":true,
          "enabledForSignedOutUsers":true,
          "enabledWithoutMsbb":true,
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  // - If it's a CCT, it must be created by GSA
  // - Must not be a supervised user
  // - Proactive help must be turned on
  bool expected_result =
      !GetParam().is_supervised_user && GetParam().proactive_help_enabled &&
      (GetParam().is_custom_tab ? GetParam().is_tab_created_by_gsa : true);
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(expected_result ? 1 : 0));
}

INSTANTIATE_TEST_SUITE_P(
    FinchStarterHeuristicConfigTestSuite,
    FinchStarterHeuristicConfigParametrizedTest,
    ValuesIn(starter_heuristic_configs_test_util::kRelevantClientStates));

}  // namespace autofill_assistant
