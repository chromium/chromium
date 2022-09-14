// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic.h"
#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/legacy_starter_heuristic_config.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class StarterHeuristicTest : public testing::Test {
 public:
  StarterHeuristicTest() = default;
  ~StarterHeuristicTest() override = default;

  // Synchronous evaluation of the heuristic for easier testing.
  base::flat_set<std::string> IsHeuristicMatchForTest(
      const StarterHeuristic& starter_heuristic,
      const GURL& url) {
    return starter_heuristic.IsHeuristicMatch(
        url, starter_heuristic.matcher_id_to_config_map_);
  }

  // Enables in-cct triggering with the specified parameters for
  // |starter_heuristic|.
  void InitDefaultHeuristic(StarterHeuristic& starter_heuristic,
                            const std::string& json_parameters) {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeaturesAndParameters(
        {{features::kAutofillAssistantUrlHeuristics,
          {{"json_parameters", json_parameters}}},
         {features::kAutofillAssistantInCCTTriggering, {}}},
        /* disabled_features = */ {});

    LegacyStarterHeuristicConfig legacy_config;
    std::vector<const StarterHeuristicConfig*> configs{&legacy_config};
    starter_heuristic.InitFromHeuristicConfigs(
        configs, &fake_platform_delegate_, &context_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(
          std::make_unique<FakeCommonDependencies>(nullptr));

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(StarterHeuristicTest, SmokeTest) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitDefaultHeuristic(*starter_heuristic, R"json(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"cart"
              }
            }
          ]
        }
        )json");

  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              ElementsAre("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              IsEmpty());
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic, GURL("invalid/cart")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest, RunHeuristicAsync) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitDefaultHeuristic(*starter_heuristic, R"json(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"cart"
              }
            }
          ]
        }
        )json");

  base::MockCallback<
      base::OnceCallback<void(const base::flat_set<std::string>&)>>
      callback;
  EXPECT_CALL(callback, Run(base::flat_set<std::string>{"FAKE_INTENT_CART"}));
  starter_heuristic->RunHeuristicAsync(GURL("https://www.example.com/cart"),
                                       callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(StarterHeuristicTest, DenylistedDomains) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitDefaultHeuristic(*starter_heuristic, R"json(
        {
          "denylistedDomains": ["example.com", "other-example.com"],
          "heuristics":[
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"cart"
              }
            }
          ]
        }
        )json");

  // URLs on denylisted domains or subdomains thereof will always fail the
  // heuristic even if they would otherwise match.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              IsEmpty());
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/cart")),
              IsEmpty());
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://subdomain.example.com/cart")),
      IsEmpty());
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              IsEmpty());
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://www.other-example.com/cart")),
      IsEmpty());

  // URLs on non-denylisted domains still work.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://allowed.com/cart")),
              ElementsAre("FAKE_INTENT_CART"));
}

TEST_F(StarterHeuristicTest, MultipleConditionSetsForSameIntent) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitDefaultHeuristic(*starter_heuristic, R"json(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"cart"
              }
            },
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"shopping-bag"
              }
            }
          ]
        }
        )json");

  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/cart")),
              ElementsAre("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/shopping-bag")),
              ElementsAre("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest, FieldTrialNotSet) {
  // Just a check that this does not crash.
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest, FieldTrialInvalid) {
  // Just a check that this does not crash.
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitDefaultHeuristic(*starter_heuristic, "invalid");

  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest, PartiallyInvalidFieldTrialsAreCompletelyIgnored) {
  // |denylistedDomains| expects an array of strings. If specified but invalid,
  // the entire configuration should be ignored.
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitDefaultHeuristic(*starter_heuristic, R"(
        {
          "denylistedDomains": [-1],
          "heuristics":[
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"cart"
              }
            }
          ]
        }
        )");

  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest,
       ConfigsContainingInvalidConditionSetsAreSilentlySkipped) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"json_parameters", R"json(
        {
          "intent":"NEW_INTENT_A",
          "heuristics":[
              {
                "conditionSet":{
                  "### INVALID ###":"whatever"
                }
              },
              {
                "conditionSet":{
                  "urlContains":"trigger-for-a"
                }
              }
          ],
          "enabledInCustomTabs":true
        }
        )json"}}},
       {features::kAutofillAssistantUrlHeuristic2, {{"json_parameters", R"json(
        {
          "intent":"NEW_INTENT_B",
          "heuristics":[
              {
                "conditionSet":{
                  "urlContains":"trigger-for-b"
                }
              }
          ],
          "enabledInCustomTabs":true
        }
        )json"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig finch_config_1{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "json_parameters", ""}};
  FinchStarterHeuristicConfig finch_config_2{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic2, "json_parameters", ""}};
  std::vector<const StarterHeuristicConfig*> configs{&finch_config_1,
                                                     &finch_config_2};
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_web_layer_ = false;
  starter_heuristic->InitFromHeuristicConfigs(configs, &fake_platform_delegate_,
                                              &context_);

  // config for NEW_INTENT_A contains both valid and invalid conditions and
  // should be skipped entirely.
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://www.example.com/trigger-for-a")),
      IsEmpty());

  // config for NEW_INTENT_B is valid and should thus work.
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://www.example.com/trigger-for-b")),
      ElementsAre("NEW_INTENT_B"));
}

TEST_F(StarterHeuristicTest, MultipleUrlHeuristicTrials) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"json(
        {
          "denylistedDomains": ["example.com", "other-example.com"],
          "heuristics":[
            {
              "intent":"LEGACY_INTENT",
              "conditionSet":{
                "urlContains":"cart"
              }
            },
            {
              "intent":"LEGACY_INTENT",
              "conditionSet":{
                "urlContains":"trolley"
              }
            }
          ]
        }
        )json"}}},
       {features::kAutofillAssistantUrlHeuristic1, {{"json_parameters", R"json(
        {
          "denylistedDomains": ["example.com", "other-example.com"],
          "intent":"NEW_INTENT_A",
          "heuristics":[
              {
                "conditionSet":{
                  "urlContains":"cart"
                }
              },
              {
                "conditionSet":{
                  "urlContains":"bag"
                }
              }
          ],
          "enabledInCustomTabs":true
        }
        )json"}}},
       {features::kAutofillAssistantUrlHeuristic2, {{"json_parameters", R"json(
        {
          "denylistedDomains": ["example.com"],
          "intent":"NEW_INTENT_B",
          "heuristics":[
              {
                "conditionSet":{
                  "urlContains":"cart"
                }
              },
              {
                "conditionSet":{
                  "urlContains":"checkout"
                }
              }
          ],
          "enabledInCustomTabs":true,
          "enabledInRegularTabs":true
        }
        )json"}}},
       {features::kAutofillAssistantInCCTTriggering, {}}},
      /* disabled_features = */ {});

  LegacyStarterHeuristicConfig legacy_config;
  FinchStarterHeuristicConfig finch_config_1{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "json_parameters", ""}};
  FinchStarterHeuristicConfig finch_config_2{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic2, "json_parameters", ""}};
  std::vector<const StarterHeuristicConfig*> configs{
      &legacy_config, &finch_config_1, &finch_config_2};
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_web_layer_ = false;
  starter_heuristic->InitFromHeuristicConfigs(configs, &fake_platform_delegate_,
                                              &context_);

  // Denylisted in all configs.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              IsEmpty());

  // Denylisted in all configs except for NEW_INTENT_B.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://other-example.com/cart")),
              ElementsAre("NEW_INTENT_B"));

  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://different.com/trolley")),
              ElementsAre("LEGACY_INTENT"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://different.com/bag")),
              ElementsAre("NEW_INTENT_A"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://different.com/checkout")),
              ElementsAre("NEW_INTENT_B"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://different.com/cart")),
              ElementsAre("LEGACY_INTENT", "NEW_INTENT_A", "NEW_INTENT_B"));

  fake_platform_delegate_.is_custom_tab_ = false;
  starter_heuristic->InitFromHeuristicConfigs(configs, &fake_platform_delegate_,
                                              &context_);
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://different.com/cart")),
              ElementsAre("NEW_INTENT_B"));
}

}  // namespace autofill_assistant
