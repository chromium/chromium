// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/legacy_starter_heuristic_config.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

class LegacyStarterHeuristicConfigTest : public testing::Test {
 public:
  LegacyStarterHeuristicConfigTest() = default;
  ~LegacyStarterHeuristicConfigTest() override = default;

 protected:
  // Convenience for tests where the contents of the heuristic don't matter.
  void InitDefaultHeuristic() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeaturesAndParameters(
        {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT",
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
  FakeStarterPlatformDelegate fake_platform_delegate_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(LegacyStarterHeuristicConfigTest, DisabledIfNoFeatureParam) {
  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, DenylistedDomains) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "denylistedDomains":["example.com", "different.com"],
          "heuristics":[
            {
              "intent":"FAKE_INTENT_A",
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetDenylistedDomains(),
              UnorderedElementsAreArray(
                  std::vector<std::string>{"example.com", "different.com"}));
}

TEST_F(LegacyStarterHeuristicConfigTest, MultipleConditionSets) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_A",
              "conditionSet":{
                "urlContains":"something"
              }
            },
            {
              "intent":"FAKE_INTENT_B",
              "conditionSet":{
                "urlContains":"different"
              }
            }
          ]
        }
        )"}}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_web_layer_ = false;
  EXPECT_EQ(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                  &context_),
            base::JSONReader::Read(R"([
            {
              "intent":"FAKE_INTENT_A",
              "conditionSet":{
                "urlContains":"something"
              }
            },
            {
              "intent":"FAKE_INTENT_B",
              "conditionSet":{
                "urlContains":"different"
              }
            }
          ])")
                ->GetList());
}

TEST_F(LegacyStarterHeuristicConfigTest, UseFirstIntentIfMultipleSpecified) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_A",
              "conditionSet":{
                "urlContains":"something"
              }
            },
            {
              "intent":"FAKE_INTENT_B",
              "conditionSet":{
                "urlContains":"different"
              }
            }
          ]
        }
        )"}}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetIntent(), Eq("FAKE_INTENT_A"));
}

TEST_F(LegacyStarterHeuristicConfigTest, OnlyEnabledInCustomTab) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {features::kAutofillAssistantInTabTriggering});
  LegacyStarterHeuristicConfig config;

  // Allowed: custom tabs created by GSA.
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));

  // Not a custom tab.
  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  // CCT not created by GSA.
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_tab_created_by_gsa_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, OnlyEnabledInRegularTab) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInTabTriggering},
      /* disabled_features = */ {features::kAutofillAssistantInCCTTriggering});
  LegacyStarterHeuristicConfig config;

  // Allowed: regular tabs.
  fake_platform_delegate_.is_custom_tab_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));

  // Not a regular tab.
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, EnabledInAllTabs) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInTabTriggering,
                                features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {});
  LegacyStarterHeuristicConfig config;

  // Allowed: regular tabs.
  fake_platform_delegate_.is_custom_tab_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));

  // Allowed: custom tabs.
  fake_platform_delegate_.is_custom_tab_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(LegacyStarterHeuristicConfigTest, OnlySignedInUsersInWeblayer) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {features::kAutofillAssistantInTabTriggering});
  LegacyStarterHeuristicConfig config;

  // For weblayer, only signed in users are allowed.
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_web_layer_ = true;
  fake_platform_delegate_.is_logged_in_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));

  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_web_layer_ = true;
  fake_platform_delegate_.is_logged_in_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, InvalidDenylistedDomains) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "denylistedDomains":[{"invalid_nested_in_object"}],
          "heuristics":[
            {
              "intent":"FAKE_INTENT_A",
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}},
       {features::kAutofillAssistantInTabTriggering, {}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, NoIntent) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "heuristics":[
            {
              "conditionSet":{
                "urlContains":"something"
              }
            }
          ]
        }
        )"}}},
       {features::kAutofillAssistantInTabTriggering, {}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, NoConditionSet) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_A",
            }
          ]
        }
        )"}}},
       {features::kAutofillAssistantInTabTriggering, {}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, EmptyConditionSets) {
  // Technically allowed, but pointless.
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
        {
          "heuristics":[]
        }
        )"}}},
       {features::kAutofillAssistantInTabTriggering, {}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig config;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
}

TEST_F(LegacyStarterHeuristicConfigTest, DisabledForSupervisedUsers) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInTabTriggering,
                                features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {});
  LegacyStarterHeuristicConfig config;

  fake_platform_delegate_.is_supervised_user_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.is_supervised_user_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(LegacyStarterHeuristicConfigTest,
       DisabledForNotAllowedForMachineLearningUsers) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInTabTriggering,
                                features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {});
  LegacyStarterHeuristicConfig config;

  fake_platform_delegate_.is_allowed_for_machine_learning_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.is_allowed_for_machine_learning_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(LegacyStarterHeuristicConfigTest, DisabledIfProactiveHelpSettingOff) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInTabTriggering,
                                features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {});
  LegacyStarterHeuristicConfig config;

  fake_platform_delegate_.proactive_help_enabled_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.proactive_help_enabled_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(LegacyStarterHeuristicConfigTest, DisabledIfMsbbOff) {
  InitDefaultHeuristic();
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /* enabled_features = */ {features::kAutofillAssistantInTabTriggering,
                                features::kAutofillAssistantInCCTTriggering},
      /* disabled_features = */ {});
  LegacyStarterHeuristicConfig config;

  fake_platform_delegate_.fake_common_dependencies_.msbb_enabled_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.fake_common_dependencies_.msbb_enabled_ = true;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

}  // namespace autofill_assistant
