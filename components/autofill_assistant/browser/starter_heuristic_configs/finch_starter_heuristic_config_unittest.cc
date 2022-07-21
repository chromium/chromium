// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

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

  FakeStarterPlatformDelegate fake_platform_delegate_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(FinchStarterHeuristicConfigTest, SmokeTest) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");

  FinchStarterHeuristicConfig config_enabled(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});
  EXPECT_THAT(
      config_enabled.GetConditionSetsForClientState(&fake_platform_delegate_),
      SizeIs(1));

  // UrlHeuristic2 was not enabled, so this should return the empty list.
  FinchStarterHeuristicConfig config_default_disabled(
      base::FeatureParam<std::string>{
          &features::kAutofillAssistantUrlHeuristic2, "some_key", ""});
  EXPECT_THAT(config_default_disabled.GetConditionSetsForClientState(
                  &fake_platform_delegate_),
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
  EXPECT_EQ(config.GetConditionSetsForClientState(&fake_platform_delegate_),
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
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  fake_platform_delegate_.is_supervised_user_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));
}

TEST_F(FinchStarterHeuristicConfigTest, DisabledIfProactiveHelpSettingOff) {
  InitDefaultHeuristic(features::kAutofillAssistantUrlHeuristic1, "some_key");
  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.proactive_help_enabled_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  fake_platform_delegate_.proactive_help_enabled_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
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
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, EnabledInCustomTabsOnly) {
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
          ],
          "enabledInCustomTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = false;
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  // In reality, these two flags should be mutually exclusive.
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_web_layer_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, EnabledInRegularTabsOnly) {
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
          ],
          "enabledInRegularTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_web_layer_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_web_layer_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, EnabledInWeblayerOnly) {
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
          ],
          "enabledInWeblayer":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_web_layer_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_web_layer_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, EnabledForSignedOutUsers) {
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
          ],
          "enabledForSignedOutUsers":true,
          "enabledInCustomTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_logged_in_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_logged_in_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_logged_in_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_logged_in_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());
}

TEST_F(FinchStarterHeuristicConfigTest, EnabledWithoutMsbb) {
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
          ],
          "enabledWithoutMsbb":true,
          "enabledInCustomTabs":true
        }
        )"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig config(base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "some_key", ""});

  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.msbb_enabled_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.msbb_enabled_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.msbb_enabled_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.msbb_enabled_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
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
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              SizeIs(2));

  fake_platform_delegate_.is_custom_tab_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_),
              IsEmpty());
}

}  // namespace autofill_assistant
