// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic.h"

#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Eq;
using ::testing::Optional;

class StarterHeuristicTest : public testing::Test {
 public:
  StarterHeuristicTest() = default;
  ~StarterHeuristicTest() override = default;

  // Synchronous evaluation of the heuristic for easier testing.
  absl::optional<std::string> IsHeuristicMatchForTest(
      const StarterHeuristic& starter_heuristic,
      const GURL& url) {
    return starter_heuristic.IsHeuristicMatch(url);
  }
};

TEST_F(StarterHeuristicTest, SmokeTest) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics, {{"json_parameters", R"(
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
        )"}});

  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              Eq("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              Eq(absl::nullopt));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic, GURL("invalid/cart")),
              Eq(absl::nullopt));
}

TEST_F(StarterHeuristicTest, RunHeuristicAsync) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics, {{"json_parameters",
                                                   R"(
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
        )"}});

  base::test::TaskEnvironment task_environment;
  base::MockCallback<base::OnceCallback<void(absl::optional<std::string>)>>
      callback;
  EXPECT_CALL(callback, Run(Optional(std::string("FAKE_INTENT_CART"))));
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  starter_heuristic->RunHeuristicAsync(GURL("https://www.example.com/cart"),
                                       callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(StarterHeuristicTest, MultipleIntentHeuristics) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics, {{"json_parameters",
                                                   R"(
        {
          "heuristics":[
            {
              "intent":"FAKE_INTENT_CART",
              "conditionSet":{
                "urlContains":"cart"
              }
            },
            {
              "intent":"FAKE_INTENT_OTHER",
              "conditionSet":{
                "urlMatches":".*other.*"
              }
            }
          ]
        }
        )"}});

  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              Eq("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/other")),
              Eq("FAKE_INTENT_OTHER"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              Eq(absl::nullopt));
  // If both match, FAKE_INTENT_CART has precedence because it was specified
  // first.
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://www.example.com/cart/other")),
      Eq("FAKE_INTENT_CART"));
}

TEST_F(StarterHeuristicTest, DenylistedDomains) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics, {{"json_parameters",
                                                   R"(
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
        )"}});

  // URLs on denylisted domains or subdomains thereof will always fail the
  // heuristic even if they would otherwise match.
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              Eq(absl::nullopt));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/cart")),
              Eq(absl::nullopt));
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://subdomain.example.com/cart")),
      Eq(absl::nullopt));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              Eq(absl::nullopt));
  EXPECT_THAT(
      IsHeuristicMatchForTest(*starter_heuristic,
                              GURL("https://www.other-example.com/cart")),
      Eq(absl::nullopt));

  // URLs on non-denylisted domains still work.
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://allowed.com/cart")),
              Eq("FAKE_INTENT_CART"));
}

TEST_F(StarterHeuristicTest, MultipleConditionSetsForSameIntent) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics, {{"json_parameters",
                                                   R"(
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
        )"}});

  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/cart")),
              Eq("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://example.com/shopping-bag")),
              Eq("FAKE_INTENT_CART"));
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com")),
              Eq(absl::nullopt));
}

TEST_F(StarterHeuristicTest, FieldTrialNotSet) {
  // Just a check that this does not crash.
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              Eq(absl::nullopt));
}

TEST_F(StarterHeuristicTest, FieldTrialInvalid) {
  // Just a check that this does not crash.
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics,
      {{"json_parameters", "invalid"}});
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              Eq(absl::nullopt));
}

TEST_F(StarterHeuristicTest, PartiallyInvalidFieldTrialsAreCompletelyIgnored) {
  // |denylistedDomains| expects an array of strings. If specified but invalid,
  // the entire configuration should be ignored.
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kAutofillAssistantUrlHeuristics, {{"json_parameters",
                                                   R"(
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
        )"}});
  auto starter_heuristic = base::MakeRefCounted<StarterHeuristic>();
  EXPECT_THAT(IsHeuristicMatchForTest(*starter_heuristic,
                                      GURL("https://www.example.com/cart")),
              Eq(absl::nullopt));
}

}  // namespace autofill_assistant
