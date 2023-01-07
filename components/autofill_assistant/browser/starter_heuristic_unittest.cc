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
#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_configs.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_starter_heuristic_config.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class StarterHeuristicTest : public testing::Test {
 public:
  StarterHeuristicTest() {
    // Settings that satisfy the shopping config requirements.
    fake_platform_delegate_.is_custom_tab_ = true;
    fake_platform_delegate_.is_web_layer_ = false;
    fake_platform_delegate_.is_logged_in_ = true;
    fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = true;
    fake_platform_delegate_.is_supervised_user_ = false;
    fake_platform_delegate_.proactive_help_enabled_ = true;
    fake_platform_delegate_.is_tab_created_by_gsa_ = true;
    fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
        "us";
  }

  ~StarterHeuristicTest() override = default;

  // Synchronous evaluation of the heuristic for easier testing.
  base::flat_set<std::string> IsHeuristicMatchForTest(
      const StarterHeuristic& starter_heuristic,
      const GURL& url) {
    return starter_heuristic.IsHeuristicMatch(
        url, starter_heuristic.matcher_id_to_config_map_);
  }

  // Enables in-cct triggering with the launched shopping config for
  // |starter_heuristic|.
  void InitShoppingHeuristic(StarterHeuristic& starter_heuristic) {
    std::vector<const StarterHeuristicConfig*> configs{
        launched_configs::GetOrCreateShoppingConfig()};
    starter_heuristic.InitFromHeuristicConfigs(
        configs, &fake_platform_delegate_, &context_);
  }

  // Enables in-cct triggering with the launched shopping and coupons configs
  // for |starter_heuristic|.
  void InitShoppingAndCouponHeuristics(StarterHeuristic& starter_heuristic) {
    std::vector<const StarterHeuristicConfig*> configs{
        launched_configs::GetOrCreateShoppingConfig(),
        launched_configs::GetOrCreateCouponsConfig()};
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
  InitShoppingHeuristic(*starter_heuristic);

  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              IsEmpty());
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic, GURL("invalid/cart")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest, RunHeuristicAsync) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitShoppingHeuristic(*starter_heuristic);

  base::MockCallback<
      base::OnceCallback<void(const base::flat_set<std::string>&)>>
      callback;
  EXPECT_CALL(callback,
              Run(base::flat_set<std::string>{"SHOPPING_ASSISTED_CHECKOUT"}));
  starter_heuristic->RunHeuristicAsync(GURL("https://www.example.com/cart"),
                                       callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(StarterHeuristicTest, DenylistedDomains) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitShoppingHeuristic(*starter_heuristic);

  // URLs on denylisted domains or subdomains thereof will always fail the
  // heuristic even if they would otherwise match.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://google.com/cart")),
              IsEmpty());
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://subdomain.google.com/cart")),
      IsEmpty());

  // URLs on non-denylisted domains still work.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/cart")),
              UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT"));
}

TEST_F(StarterHeuristicTest, MultipleConditionSetsForSameIntent) {
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  InitShoppingAndCouponHeuristics(*starter_heuristic);

  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://example.com/cart")),
      UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT", "FIND_COUPONS"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://google.com/cart")),
              IsEmpty());
}

TEST_F(StarterHeuristicTest, NotInitializedDoesntCrash) {
  // Just a check that this does not crash.
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
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
      UnorderedElementsAre("NEW_INTENT_B"));
}

TEST_F(StarterHeuristicTest,
       MultipleHeuristicTrialsSideBySideWithLaunchedConfigs) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeaturesAndParameters(
      {{features::kAutofillAssistantUrlHeuristic1, {{"json_parameters", R"json(
        {
          "denylistedDomains": ["google.com", "example.com"],
          "intent":"NEW_INTENT_A",
          "heuristics":[
              {
                "conditionSet":{
                  "urlContains":"trigger-for-a"
                }
              },
              {
                "conditionSet":{
                  "urlContains":"trigger-for-a-and-b"
                }
              }
          ],
          "enabledInCustomTabs":true
        }
        )json"}}},
       {features::kAutofillAssistantUrlHeuristic2, {{"json_parameters", R"json(
        {
          "denylistedDomains": ["google.com"],
          "intent":"NEW_INTENT_B",
          "heuristics":[
              {
                "conditionSet":{
                  "urlContains":"trigger-for-b"
                }
              },
              {
                "conditionSet":{
                  "urlContains":"trigger-for-a-and-b"
                }
              }
          ],
          "enabledInCustomTabs":true,
          "enabledInRegularTabs":true
        }
        )json"}}},
       {features::kAutofillAssistantUrlHeuristic3, {{"json_parameters", R"json(
        {
          "denylistedDomains": ["google.com"],
          "intent":"SHOPPING_ASSISTED_CHECKOUT",
          "heuristics":[
              {
                "conditionSet":{
                  "urlContains":"einkaufswagen"
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
        )json"}}}},
      /* disabled_features = */ {});

  FinchStarterHeuristicConfig finch_config_1{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic1, "json_parameters", ""}};
  FinchStarterHeuristicConfig finch_config_2{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic2, "json_parameters", ""}};
  FinchStarterHeuristicConfig finch_config_3{base::FeatureParam<std::string>{
      &features::kAutofillAssistantUrlHeuristic3, "json_parameters", ""}};

  std::vector<const StarterHeuristicConfig*> configs{
      launched_configs::GetOrCreateShoppingConfig(),
      launched_configs::GetOrCreateCouponsConfig(), &finch_config_1,
      &finch_config_2, &finch_config_3};
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  starter_heuristic->InitFromHeuristicConfigs(configs, &fake_platform_delegate_,
                                              &context_);

  // Denylisted in all configs.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.google.com/cart")),
              IsEmpty());

  // Denylisted in A, but allowed in the launched configs.
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://example.com/cart")),
      UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT", "FIND_COUPONS"));

  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://different.com/cart/trigger-for-b")),
      UnorderedElementsAre("NEW_INTENT_B", "SHOPPING_ASSISTED_CHECKOUT",
                           "FIND_COUPONS"));
  EXPECT_THAT(IsHeuristicMatchForTest(
                  *starter_heuristic,
                  GURL("https://different.com/trigger-for-a/checkout")),
              UnorderedElementsAre("NEW_INTENT_A", "SHOPPING_ASSISTED_CHECKOUT",
                                   "FIND_COUPONS"));
  EXPECT_THAT(
      IsHeuristicMatchForTest(
          *starter_heuristic,
          GURL("https://different.com/cart/trigger-for-a-and-b")),
      UnorderedElementsAre("NEW_INTENT_A", "NEW_INTENT_B",
                           "SHOPPING_ASSISTED_CHECKOUT", "FIND_COUPONS"));
  EXPECT_THAT(IsHeuristicMatchForTest(
                  *starter_heuristic,
                  GURL("https://different.com/trigger-for-a-and-b")),
              UnorderedElementsAre("NEW_INTENT_A", "NEW_INTENT_B"));

  // Heuristic 3 has some overlap with the launched configs.
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://example.com/einkaufswagen")),
      UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT"));
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://example.com/bag")),
      UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT", "FIND_COUPONS"));

  fake_platform_delegate_.is_custom_tab_ = false;
  starter_heuristic->InitFromHeuristicConfigs(configs, &fake_platform_delegate_,
                                              &context_);
  EXPECT_THAT(IsHeuristicMatchForTest(
                  *starter_heuristic,
                  GURL("https://different.com/cart/trigger-for-a-and-b")),
              UnorderedElementsAre("NEW_INTENT_B"));
}

}  // namespace autofill_assistant
