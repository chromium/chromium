// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/safety_list_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/actor/core/actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

using Decision = SafetyListManager::Decision;
using ParseResult = SafetyListManager::ParseResult;

class SafetyListManagerTest : public ::testing::TestWithParam<bool> {
 public:
  SafetyListManagerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kGlicCrossOriginNavigationGating,
             {
                 {"enforce_component_updater_block_list_entries",
                  enforce_component_updater_blocklist() ? "true" : "false"},
             }},
        },
        /*disabled_features=*/{});
    manager_ = SafetyListManager::CreateForTesting();
  }

 protected:
  SafetyListManager& manager() { return *manager_; }

  Decision Find(const GURL& source, const GURL& destination) {
    base::test::TestFuture<Decision> future;
    manager().Find(source, destination, future.GetCallback());
    return future.Get();
  }

  void SetSafetyLists(std::string json) {
    SetSafetyListsForTesting(&manager(), std::move(json));
  }

  bool enforce_component_updater_blocklist() { return GetParam(); }

  Decision ExpectedBlocklistDecision() {
    return enforce_component_updater_blocklist() ? Decision::kBlock
                                                 : Decision::kNone;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // `manager_` delays its construction until after `scoped_feature_list_` has
  // been initialized. This ensures that the Finch feature flags are correctly
  // set when `SafetyListManager`'s constructor is called.
  std::unique_ptr<SafetyListManager> manager_ = nullptr;
  base::test::TaskEnvironment task_environment_;
};

TEST_P(SafetyListManagerTest, DefaultInstance) {
  EXPECT_EQ(
      Find(GURL("https://anything.com"), GURL("https://www.googleplex.com")),
      Decision::kNone);
  EXPECT_EQ(Find(GURL("https://anything.com"), GURL("https://corp.google.com")),
            Decision::kNone);
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_LazyUntilFirstFind) {
  base::HistogramTester histogram_tester;
  bool producer_called = false;
  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        producer_called = true;
        return R"json({
          "navigation_allowed": [{ "from": "a.com", "to": "b.com" }],
          "navigation_blocked": []
        })json";
      }));

  // Setting the callback should not invoke the producer or parse the safety
  // lists before `Find()` is called.
  EXPECT_FALSE(producer_called);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationAllowed", 0);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationBlocked", 0);

  // The first call to `Find()` triggers the producer and parses the JSON.
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);
  EXPECT_TRUE(producer_called);
  histogram_tester.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      1);
  histogram_tester.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked", ParseResult::kSuccess,
      1);

  // Subsequent `Find()` calls reuse the parsed settings without re-parsing.
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationAllowed", 1);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationBlocked", 1);
}

TEST_P(SafetyListManagerTest,
       ParseSafetyLists_MultipleUpdatesBeforeFindOnlyUsesLatest) {
  base::HistogramTester histogram_tester;
  bool first_producer_called = false;
  bool second_producer_called = false;

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        first_producer_called = true;
        return R"json({
          "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
        })json";
      }));

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        second_producer_called = true;
        return R"json({
          "navigation_allowed": [{ "from": "c.com", "to": "d.com" }]
        })json";
      }));

  EXPECT_FALSE(first_producer_called);
  EXPECT_FALSE(second_producer_called);

  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("https://c.com"), GURL("https://d.com")),
            Decision::kAllow);
  EXPECT_FALSE(first_producer_called);
  EXPECT_TRUE(second_producer_called);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationAllowed", 1);
}

TEST_P(SafetyListManagerTest,
       ParseSafetyLists_ConcurrentFindsShareSingleParse) {
  base::HistogramTester histogram_tester;
  int producer_call_count = 0;
  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        ++producer_call_count;
        return R"json({
          "navigation_allowed": [{ "from": "a.com", "to": "b.com" }],
          "navigation_blocked": [{ "from": "c.com", "to": "d.com" }]
        })json";
      }));

  base::test::TestFuture<Decision> future1;
  base::test::TestFuture<Decision> future2;
  base::test::TestFuture<Decision> future3;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future1.GetCallback());
  manager().Find(GURL("https://c.com"), GURL("https://d.com"),
                 future2.GetCallback());
  manager().Find(GURL("https://e.com"), GURL("https://f.com"),
                 future3.GetCallback());

  EXPECT_EQ(future1.Get(), Decision::kAllow);
  EXPECT_EQ(future2.Get(), ExpectedBlocklistDecision());
  EXPECT_EQ(future3.Get(), Decision::kNone);
  EXPECT_EQ(producer_call_count, 1);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationAllowed", 1);
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_NulloptProducer) {
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
  })json");
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);

  base::HistogramTester histogram_tester;
  manager().SetLoadSafetyListsClosure(base::BindOnce(
      []() -> std::optional<std::string> { return std::nullopt; }));
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationAllowed", 0);
  histogram_tester.ExpectTotalCount(
      "Actor.SafetyListParseResult.NavigationBlocked", 0);
}

TEST_P(SafetyListManagerTest, FindBeforeSetLoadSafetyListsClosure) {
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);

  // Setting a failing closure (nullopt) after an initial Find.
  manager().SetLoadSafetyListsClosure(base::BindOnce(
      []() -> std::optional<std::string> { return std::nullopt; }));
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);

  // Setting a failing closure (invalid JSON) after a failed Find.
  SetSafetyLists("not valid json");
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);

  // Setting a succeeding closure after failed Finds.
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
  })json");
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);
}

TEST_P(SafetyListManagerTest,
       ParseSafetyLists_MultipleUpdatesBeforeFind_FailureThenSuccess) {
  bool first_producer_called = false;
  bool second_producer_called = false;

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        first_producer_called = true;
        return std::nullopt;
      }));

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        second_producer_called = true;
        return R"json({
          "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
        })json";
      }));

  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);
  EXPECT_FALSE(first_producer_called);
  EXPECT_TRUE(second_producer_called);
}

TEST_P(SafetyListManagerTest,
       ParseSafetyLists_MultipleUpdatesBeforeFind_SuccessThenFailure) {
  bool first_producer_called = false;
  bool second_producer_called = false;

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        first_producer_called = true;
        return R"json({
          "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
        })json";
      }));

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        second_producer_called = true;
        return "invalid json";
      }));

  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
  EXPECT_FALSE(first_producer_called);
  EXPECT_TRUE(second_producer_called);
}

TEST_P(SafetyListManagerTest,
       ParseSafetyLists_SequentialFailureAndSuccessTransitions) {
  int nullopt_producer_calls = 0;
  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        ++nullopt_producer_calls;
        return std::nullopt;
      }));

  // First Find runs the failing closure; subsequent Find returns kNone
  // immediately without re-invoking the closure.
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
  EXPECT_EQ(nullopt_producer_calls, 1);

  // Transition from empty state to populated state.
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
  })json");
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);

  // Invalid JSON doesn't overwrite valid data.
  SetSafetyLists("not valid json");
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kAllow);

  // New data can overwrite old data.
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "c.com", "to": "d.com" }]
  })json");
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("https://c.com"), GURL("https://d.com")),
            Decision::kAllow);
}

TEST_P(SafetyListManagerTest,
       ParseSafetyLists_ConcurrentFindsOnFailureResolveToNone) {
  int producer_call_count = 0;
  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        ++producer_call_count;
        return "invalid json";
      }));

  base::test::TestFuture<Decision> future1;
  base::test::TestFuture<Decision> future2;
  base::test::TestFuture<Decision> future3;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future1.GetCallback());
  manager().Find(GURL("https://c.com"), GURL("https://d.com"),
                 future2.GetCallback());
  manager().Find(GURL("https://e.com"), GURL("https://f.com"),
                 future3.GetCallback());

  EXPECT_EQ(future1.Get(), Decision::kNone);
  EXPECT_EQ(future2.Get(), Decision::kNone);
  EXPECT_EQ(future3.Get(), Decision::kNone);
  EXPECT_EQ(producer_call_count, 1);
}

TEST_P(SafetyListManagerTest,
       InFlightParseReplacedByNewClosure_FailureThenSuccess) {
  manager().SetLoadSafetyListsClosure(base::BindOnce(
      []() -> std::optional<std::string> { return std::nullopt; }));

  base::test::TestFuture<Decision> future1;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future1.GetCallback());

  // While the first (failing) parse is in flight, provide a valid list.
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
  })json");

  base::test::TestFuture<Decision> future2;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future2.GetCallback());

  EXPECT_EQ(future1.Get(), Decision::kAllow);
  EXPECT_EQ(future2.Get(), Decision::kAllow);
}

TEST_P(SafetyListManagerTest,
       InFlightParseReplacedByNewClosure_SuccessThenFailure) {
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
  })json");

  base::test::TestFuture<Decision> future1;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future1.GetCallback());

  // While the first (succeeding) parse is in flight, replace with a failing
  // closure.
  manager().SetLoadSafetyListsClosure(base::BindOnce(
      []() -> std::optional<std::string> { return std::nullopt; }));

  base::test::TestFuture<Decision> future2;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future2.GetCallback());

  EXPECT_EQ(future1.Get(), Decision::kNone);
  EXPECT_EQ(future2.Get(), Decision::kNone);
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
}

TEST_P(SafetyListManagerTest,
       InFlightParseReplacedByNewClosure_SuccessThenSuccess) {
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "a.com", "to": "b.com" }]
  })json");

  base::test::TestFuture<Decision> future1;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future1.GetCallback());

  // While the first parse is in flight, replace with a newer valid list.
  SetSafetyLists(R"json({
    "navigation_allowed": [{ "from": "c.com", "to": "d.com" }]
  })json");

  base::test::TestFuture<Decision> future2;
  manager().Find(GURL("https://c.com"), GURL("https://d.com"),
                 future2.GetCallback());

  EXPECT_EQ(future1.Get(), Decision::kNone);
  EXPECT_EQ(future2.Get(), Decision::kAllow);
  EXPECT_EQ(Find(GURL("https://a.com"), GURL("https://b.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("https://c.com"), GURL("https://d.com")),
            Decision::kAllow);
}

TEST_P(SafetyListManagerTest,
       InFlightParseReplacedByNewClosure_FailureThenFailure) {
  bool second_called = false;
  manager().SetLoadSafetyListsClosure(base::BindOnce(
      []() -> std::optional<std::string> { return std::nullopt; }));

  base::test::TestFuture<Decision> future1;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future1.GetCallback());

  manager().SetLoadSafetyListsClosure(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        second_called = true;
        return "invalid json";
      }));

  base::test::TestFuture<Decision> future2;
  manager().Find(GURL("https://a.com"), GURL("https://b.com"),
                 future2.GetCallback());

  EXPECT_EQ(future1.Get(), Decision::kNone);
  EXPECT_EQ(future2.Get(), Decision::kNone);
  EXPECT_TRUE(second_called);
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_Validity) {
  struct InvalidListTestCase {
    std::string desc;
    std::string json;
    ParseResult expected_allowed;
    ParseResult expected_blocked;
    size_t expected_allowed_count;
    size_t expected_blocked_count;
  } kTestCases[] = {
      {
          "malformed json",
          "this isn't json",
          ParseResult::kInvalidJson,
          ParseResult::kInvalidJson,
          0u,
          0u,
      },
      {
          "not_dictionary",
          R"json({
            "navigation_allowed": [ "{}" ],
            "navigation_blocked": [ "{}" ]
          })json",
          ParseResult::kJsonListValueNotADictionary,
          ParseResult::kJsonListValueNotADictionary,
          0u,
          0u,
      },
      {
          "top_level_not_dictionary",
          R"json([])json",
          ParseResult::kInvalidJson,
          ParseResult::kInvalidJson,
          0u,
          0u,
      },
      {
          "key_value_not_a_list",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": 123
          })json",
          ParseResult::kJsonKeyValueNotAList,
          ParseResult::kJsonKeyValueNotAList,
          0u,
          0u,
      },
      {
          "to_field_missing",
          R"json({
            "navigation_allowed": [ { "from": "a.com" } ],
            "navigation_blocked": [ { "from": "a.com" } ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "from_field_missing",
          R"json({
            "navigation_allowed": [ { "to": "b.com" } ],
            "navigation_blocked": [ { "to": "b.com" } ]
          })json",
          ParseResult::kInvalidFromField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "to_field_not_string",
          R"json({
            "navigation_allowed": [{"from": "a.com", "to": 123}],
            "navigation_blocked": [{"from": "a.com", "to": 123}]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "from_field_not_string",
          R"json({
            "navigation_allowed": [{ "from": 123, "to": "b.com" }],
            "navigation_blocked": [{ "from": 123, "to": "b.com" }]
          })json",
          ParseResult::kInvalidFromField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "to_value_not_valid_pattern",
          R"json({
            "navigation_allowed": [{ "from": "a.com", "to": "b.*.com" }],
            "navigation_blocked": [{ "from": "a.com", "to": "b.*.com" }]
          })json",
          ParseResult::kInvalidToUrlPattern,
          ParseResult::kInvalidToUrlPattern,
          0u,
          0u,
      },
      {
          "from_value_not_valid_pattern",
          R"json({
            "navigation_allowed": [{ "from": "a.*.com", "to": "b.com" }],
            "navigation_blocked": [{ "from": "a.*.com", "to": "b.com" }]
          })json",
          ParseResult::kInvalidFromUrlPattern,
          ParseResult::kInvalidFromUrlPattern,
          0u,
          0u,
      },
      {
          "empty_lists",
          R"json({ "navigation_allowed": [], "navigation_blocked": [] })json",
          ParseResult::kSuccess,
          ParseResult::kSuccess,
          0u,
          0u,
      },
      {
          "allowed_valid_blocked_invalid_field",
          R"json({
            "navigation_allowed": [{ "from": "a.com", "to": "b.com" }],
            "navigation_blocked": [{ "from": "a.com" }]
          })json",
          ParseResult::kSuccess,
          ParseResult::kInvalidToField,
          1u,
          0u,
      },
      {
          "allowed_invalid_type_blocked_valid",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": [{ "from": "a.com", "to": "b.com" }]
          })json",
          ParseResult::kJsonKeyValueNotAList,
          ParseResult::kSuccess,
          0u,
          1u,
      },
      {
          "mixed_invalid_fields",
          R"json({
            "navigation_allowed": [{ "from": "a.com" }],
            "navigation_blocked": [{ "to": "b.com" }]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "mixed_invalid_types",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": [ "{}" ]
          })json",
          ParseResult::kJsonKeyValueNotAList,
          ParseResult::kJsonListValueNotADictionary,
          0u,
          0u,
      },
      {
          "multiple_valid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com", "to": "b.com" },
              { "from": "c.com", "to": "d.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com", "to": "f.com" },
              { "from": "g.com", "to": "h.com" }
            ]
          })json",
          ParseResult::kSuccess,
          ParseResult::kSuccess,
          2u,
          2u,
      },
      {
          "multiple_invalid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com" },
              { "to": "d.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com" },
              { "to": "h.com" }
            ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "mixed_valid_invalid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com", "to": "b.com" },
              { "from": "c.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com", "to": "f.com" },
              { "to": "h.com" }
            ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "allowed_partially_valid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com", "to": "b.com" },
              { "from": "c.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com", "to": "f.com" }
            ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kSuccess,
          0u,
          1u,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::HistogramTester histogram_tester;
    SetSafetyLists(test_case.json);
    // Trigger lazy parsing via `Find`.
    Find(GURL("https://example.com"), GURL("https://example.org"));

    histogram_tester.ExpectUniqueSample(
        "Actor.SafetyListParseResult.NavigationAllowed",
        test_case.expected_allowed, 1);
    histogram_tester.ExpectUniqueSample(
        "Actor.SafetyListParseResult.NavigationBlocked",
        test_case.expected_blocked, 1);
  }
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_ValidPatterns) {
  base::HistogramTester histogram_tester;
  SetSafetyLists(R"json(
    {
      "navigation_allowed": [
        { "from": "[*.]google.com", "to": "youtube.com" },
        { "from": "foo.com", "to": "[*.]bar.com" },
        { "from": "https://a.com:8080", "to": "https://*" },
        { "from": "127.0.0.1", "to": "*" }
      ],
      "navigation_blocked": [
        { "from": "blocked.com", "to": "not-allowed.com"}
      ]
    }
  )json");
  EXPECT_EQ(Find(GURL("https://www.google.com"), GURL("https://youtube.com")),
            Decision::kAllow);
  EXPECT_EQ(Find(GURL("http://foo.com"), GURL("https://sub.bar.com")),
            Decision::kAllow);
  EXPECT_EQ(Find(GURL("https://a.com:8080"), GURL("http://b.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("https://a.com:8080"), GURL("https://b.com")),
            Decision::kAllow);
  EXPECT_EQ(Find(GURL("http://127.0.0.1"), GURL("http://localhost")),
            Decision::kAllow);

  EXPECT_EQ(Find(GURL("https://blocked.com"), GURL("https://not-allowed.com")),
            ExpectedBlocklistDecision());
  histogram_tester.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      1);
  histogram_tester.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked", ParseResult::kSuccess,
      1);
}

TEST_P(SafetyListManagerTest, ParseBlockLists_MultipleParses) {
  base::HistogramTester histogram_tester;
  SetSafetyLists(R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]google.com", "to": "youtube.com" },
        { "from": "foo.com", "to": "[*.]bar.com" }
      ]
    }
  )json");
  EXPECT_EQ(Find(GURL("https://www.google.com"), GURL("https://youtube.com")),
            ExpectedBlocklistDecision());
  EXPECT_EQ(Find(GURL("http://foo.com"), GURL("https://sub.bar.com")),
            ExpectedBlocklistDecision());

  SetSafetyLists(R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]yahoo.com", "to": "vimeo.com" },
        { "from": "bar.com", "to": "[*.]foo.com" }
      ]
    }
  )json");
  EXPECT_EQ(Find(GURL("https://www.google.com"), GURL("https://youtube.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("http://foo.com"), GURL("https://sub.bar.com")),
            Decision::kNone);
  EXPECT_EQ(Find(GURL("https://www.yahoo.com"), GURL("https://vimeo.com")),
            ExpectedBlocklistDecision());
  EXPECT_EQ(Find(GURL("http://bar.com"), GURL("https://sub.foo.com")),
            ExpectedBlocklistDecision());
  histogram_tester.ExpectBucketCount(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      2);
  histogram_tester.ExpectBucketCount(
      "Actor.SafetyListParseResult.NavigationBlocked", ParseResult::kSuccess,
      2);
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_BlockedListInvalid) {
  base::HistogramTester histogram_tester;
  SetSafetyLists(R"json(
    {
      "navigation_allowed": [],
      "navigation_blocked": [
        { "from": "a.*.com", "to": "b.com" }
      ]
    }
  )json");
  Find(GURL("https://a.com"), GURL("https://b.com"));
  histogram_tester.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked",
      ParseResult::kInvalidFromUrlPattern, 1);
  histogram_tester.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      1);
}

TEST_P(SafetyListManagerTest, Find) {
  const struct {
    std::string desc;
    std::string json;
    std::string from_url;
    std::string to_url;
    Decision expected;
  } kTestCases[] = {
      {
          "source wildcard subdomain match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "[*.]a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://sub.a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "source wildcard root match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "[*.]a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "source wildcard match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "source wildcard subdomain mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "[*.]a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://other.com",
          "https://b.com",
          Decision::kNone,
      },
      {
          "destination wildcard subdomain match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "[*.]b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://sub.b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "destination wildcard match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "*" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "destination wildcard root match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "[*.]b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "destination wildcard subdomain mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "[*.]b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://other.com",
          Decision::kNone,
      },
      {
          "no wildcard origin-scoped match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://b.com" }
              ]
            }
          )json",
          "https://a.com/foo/bar",
          "https://b.com:443",
          ExpectedBlocklistDecision(),
      },
      {
          "no wildcard origin-scoped host mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://other.com",
          Decision::kNone,
      },
      {
          "no wildcard origin-scoped scheme mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://b.com" }
              ]
            }
          )json",
          "https://a.com",
          "http://b.com",
          Decision::kNone,
      },
      {
          "no wildcard origin-scoped port omitted means wildcard",
          R"json(
            {
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com:8080",
          ExpectedBlocklistDecision(),
      },
      {
          "both mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://c.com",
          "https://d.com",
          Decision::kNone,
      },
      {
          "multiple entries, single list, match one",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" },
                { "from": "c.com", "to": "d.com" }
              ]
            }
          )json",
          "https://c.com",
          "https://d.com",
          ExpectedBlocklistDecision(),
      },
      {
          "multiple origin-scoped entries, both lists, match one",
          R"json(
            {
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://b.com" },
                { "from": "https://c.com", "to": "https://d.com" }
              ],
              "navigation_allowed": [
                { "from": "https://e.com", "to": "https://f.com" },
                { "from": "https://g.com", "to": "https://h.com" }
              ]
            }
          )json",
          "https://e.com",
          "https://f.com",
          Decision::kAllow,
      },
      {
          "multiple entries, both lists, match one",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" },
                { "from": "c.com", "to": "d.com" }
              ],
              "navigation_allowed": [
                { "from": "e.com", "to": "f.com" },
                { "from": "g.com", "to": "h.com" }
              ]
            }
          )json",
          "https://e.com",
          "https://f.com",
          Decision::kAllow,
      },
      {
          "overlapping entries, specific allow, generic block",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kAllow,
      },
      {
          "overlapping entries, generic allow, specific block",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "*", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "overlapping entries, equal specificities -> blocklist wins",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "*", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "origin-scoped overlapping entries -> blocklist wins",
          R"json(
            {
              "navigation_allowed": [
                { "from": "https://a.com", "to": "https://b.com" }
              ],
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          ExpectedBlocklistDecision(),
      },
      {
          "origin-scoped allowlist overrides more general setting",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "https://a.com:443", "to": "https://b.com:443" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kAllow,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    SetSafetyLists(test_case.json);
    EXPECT_EQ(Find(GURL(test_case.from_url), GURL(test_case.to_url)),
              test_case.expected);
  }
}

TEST_P(SafetyListManagerTest, Find_SameOrigin) {
  const struct {
    std::string desc;
    std::string json;
    Decision expected;
  } kTestCases[] = {
      {
          "wildcard scheme",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "a.com" }
              ]
            }
          )json",
          ExpectedBlocklistDecision(),
      },
      {
          "wildcard source",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "a.com" }
              ]
            }
          )json",
          ExpectedBlocklistDecision(),
      },
      {
          "wildcard destination",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "*" }
              ]
            }
          )json",
          ExpectedBlocklistDecision(),
      },
      {
          "all wildcards",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "*" }
              ]
            }
          )json",
          ExpectedBlocklistDecision(),
      },
      {
          "origin scoped",
          R"json(
            {
              "navigation_blocked": [
                { "from": "https://a.com", "to": "https://a.com" }
              ]
            }
          )json",
          ExpectedBlocklistDecision(),
      },
  };

  GURL url("https://a.com");

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    SetSafetyLists(test_case.json);
    EXPECT_EQ(Find(url, url), test_case.expected);
  }
}

INSTANTIATE_TEST_SUITE_P(All, SafetyListManagerTest, testing::Bool());

}  // namespace
}  // namespace actor
