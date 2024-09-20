// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::MaxEventLevelReports;
using ::attribution_reporting::TriggerSpecs;
using ::attribution_reporting::mojom::SourceType;
using ::base::test::ErrorIs;

MATCHER_P(IsValidAndHolds, v, "") {
  if (!arg.IsValid()) {
    *result_listener << "which is invalid";
    return false;
  }

  return ExplainMatchResult(v, arg.ValueOrDie(), result_listener);
}

TEST(PrivacyMathTest, BinomialCoefficient) {
  // Test cases generated via a python program using scipy.special.comb.
  struct {
    uint32_t n;
    uint32_t k;
    uint32_t expected;
  } kTestCases[]{
      // All cases for n and k in [0, 10).
      // clang-format off
      {0, 0, 1},  {0, 1, 0},  {0, 2, 0},  {0, 3, 0},  {0, 4, 0},   {0, 5, 0},
      {0, 6, 0},  {0, 7, 0},  {0, 8, 0},  {0, 9, 0},  {1, 0, 1},   {1, 1, 1},
      {1, 2, 0},  {1, 3, 0},  {1, 4, 0},  {1, 5, 0},  {1, 6, 0},   {1, 7, 0},
      {1, 8, 0},  {1, 9, 0},  {2, 0, 1},  {2, 1, 2},  {2, 2, 1},   {2, 3, 0},
      {2, 4, 0},  {2, 5, 0},  {2, 6, 0},  {2, 7, 0},  {2, 8, 0},   {2, 9, 0},
      {3, 0, 1},  {3, 1, 3},  {3, 2, 3},  {3, 3, 1},  {3, 4, 0},   {3, 5, 0},
      {3, 6, 0},  {3, 7, 0},  {3, 8, 0},  {3, 9, 0},  {4, 0, 1},   {4, 1, 4},
      {4, 2, 6},  {4, 3, 4},  {4, 4, 1},  {4, 5, 0},  {4, 6, 0},   {4, 7, 0},
      {4, 8, 0},  {4, 9, 0},  {5, 0, 1},  {5, 1, 5},  {5, 2, 10},  {5, 3, 10},
      {5, 4, 5},  {5, 5, 1},  {5, 6, 0},  {5, 7, 0},  {5, 8, 0},   {5, 9, 0},
      {6, 0, 1},  {6, 1, 6},  {6, 2, 15}, {6, 3, 20}, {6, 4, 15},  {6, 5, 6},
      {6, 6, 1},  {6, 7, 0},  {6, 8, 0},  {6, 9, 0},  {7, 0, 1},   {7, 1, 7},
      {7, 2, 21}, {7, 3, 35}, {7, 4, 35}, {7, 5, 21}, {7, 6, 7},   {7, 7, 1},
      {7, 8, 0},  {7, 9, 0},  {8, 0, 1},  {8, 1, 8},  {8, 2, 28},  {8, 3, 56},
      {8, 4, 70}, {8, 5, 56}, {8, 6, 28}, {8, 7, 8},  {8, 8, 1},   {8, 9, 0},
      {9, 0, 1},  {9, 1, 9},  {9, 2, 36}, {9, 3, 84}, {9, 4, 126}, {9, 5, 126},
      {9, 6, 84}, {9, 7, 36}, {9, 8, 9},  {9, 9, 1},
      // clang-format on

      // A few larger cases:
      {30, 3, 4060},
      {100, 2, 4950},
      {100, 5, 75287520},
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(internal::BinomialCoefficient(test_case.n, test_case.k),
                IsValidAndHolds(test_case.expected))
        << "n=" << test_case.n << ", k=" << test_case.k;
  }
}

TEST(PrivacyMathTest, GetKCombinationAtIndex) {
  // Test cases vetted via an equivalent calculator:
  // https://planetcalc.com/8592/
  struct {
    uint32_t index;
    uint32_t k;
    std::vector<uint32_t> expected;
  } kTestCases[]{
      {0, 0, {}},

      // clang-format off
      {0, 1, {0}},        {1, 1, {1}},        {2, 1, {2}},
      {3, 1, {3}},        {4, 1, {4}},        {5, 1, {5}},
      {6, 1, {6}},        {7, 1, {7}},        {8, 1, {8}},
      {9, 1, {9}},        {10, 1, {10}},      {11, 1, {11}},
      {12, 1, {12}},      {13, 1, {13}},      {14, 1, {14}},
      {15, 1, {15}},      {16, 1, {16}},      {17, 1, {17}},
      {18, 1, {18}},      {19, 1, {19}},

      {0, 2, {1, 0}},     {1, 2, {2, 0}},     {2, 2, {2, 1}},
      {3, 2, {3, 0}},     {4, 2, {3, 1}},     {5, 2, {3, 2}},
      {6, 2, {4, 0}},     {7, 2, {4, 1}},     {8, 2, {4, 2}},
      {9, 2, {4, 3}},     {10, 2, {5, 0}},    {11, 2, {5, 1}},
      {12, 2, {5, 2}},    {13, 2, {5, 3}},    {14, 2, {5, 4}},
      {15, 2, {6, 0}},    {16, 2, {6, 1}},    {17, 2, {6, 2}},
      {18, 2, {6, 3}},    {19, 2, {6, 4}},

      {0, 3, {2, 1, 0}},  {1, 3, {3, 1, 0}},  {2, 3, {3, 2, 0}},
      {3, 3, {3, 2, 1}},  {4, 3, {4, 1, 0}},  {5, 3, {4, 2, 0}},
      {6, 3, {4, 2, 1}},  {7, 3, {4, 3, 0}},  {8, 3, {4, 3, 1}},
      {9, 3, {4, 3, 2}},  {10, 3, {5, 1, 0}}, {11, 3, {5, 2, 0}},
      {12, 3, {5, 2, 1}}, {13, 3, {5, 3, 0}}, {14, 3, {5, 3, 1}},
      {15, 3, {5, 3, 2}}, {16, 3, {5, 4, 0}}, {17, 3, {5, 4, 1}},
      {18, 3, {5, 4, 2}}, {19, 3, {5, 4, 3}},
      // clang-format on

      {2924, 3, {26, 25, 24}},
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(internal::GetKCombinationAtIndex(test_case.index, test_case.k),
              test_case.expected)
        << "index=" << test_case.index << ", k=" << test_case.k;
  }
}

// Simple stress test to make sure that GetKCombinationAtIndex is returning
// combinations uniquely indexed by the given index, i.e. there are never any
// repeats.
TEST(PrivacyMathTest, GetKCombination_NoRepeats) {
  for (uint32_t k = 1u; k < 5u; k++) {
    std::set<std::vector<uint32_t>> seen_combinations;
    for (uint32_t index = 0; index < 3000; index++) {
      std::vector<uint32_t> combination =
          internal::GetKCombinationAtIndex(index, k);
      EXPECT_TRUE(seen_combinations.insert(std::move(combination)).second)
          << "index=" << index << ", k=" << k;
    }
  }
}

// The k-combination at a given index is the unique set of k positive integers
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0 such that
// `index` = \sum_{i=1}^k {a_i}\choose{i}
TEST(PrivacyMathTest, GetKCombination_MatchesDefinition) {
  for (uint32_t k = 1; k < 5; k++) {
    for (uint32_t index = 0; index < 3000; index++) {
      const std::vector<uint32_t> combination =
          internal::GetKCombinationAtIndex(index, k);
      base::CheckedNumeric<uint32_t> sum = 0;
      for (uint32_t i = 0; i < k; i++) {
        sum += internal::BinomialCoefficient(combination[i], k - i);
      }
      EXPECT_THAT(sum, IsValidAndHolds(index));
    }
  }
}

TEST(PrivacyMathTest, GetNumberOfStarsAndBarsSequences) {
  EXPECT_THAT(internal::GetNumberOfStarsAndBarsSequences(/*num_stars=*/1u,
                                                         /*num_bars=*/2u),
              IsValidAndHolds(3u));

  EXPECT_THAT(internal::GetNumberOfStarsAndBarsSequences(/*num_stars=*/3u,
                                                         /*num_bars=*/24u),
              IsValidAndHolds(2925u));
}

TEST(PrivacyMathTest, GetStarIndices) {
  const struct {
    uint32_t num_stars;
    uint32_t num_bars;
    uint32_t sequence_index;
    std::vector<uint32_t> expected;
  } kTestCases[] = {
      {1, 2, 2, {2}},
      {3, 24, 23, {6, 3, 0}},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(internal::GetStarIndices(test_case.num_stars, test_case.num_bars,
                                       test_case.sequence_index),
              test_case.expected);
  }
}

TEST(PrivacyMathTest, GetBarsPrecedingEachStar) {
  const struct {
    std::vector<uint32_t> star_indices;
    std::vector<uint32_t> expected;
  } kTestCases[] = {
      {{2}, {2}},
      {{6, 3, 0}, {4, 2, 0}},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(internal::GetBarsPrecedingEachStar(test_case.star_indices),
              test_case.expected);
  }
}

// Adapted from
// https://github.com/WICG/attribution-reporting-api/blob/ab43f8c989cf881ffd7a7f71801b98d649ed164a/flexible-event/privacy.test.ts#L76C1-L82C2
TEST(PrivacyMathTest, BinaryEntropy) {
  const struct {
    double x;
    double expected;
  } kTestCases[] = {
      {.x = 0, .expected = 0},
      {.x = 0.5, .expected = 1},
      {.x = 1, .expected = 0},
      {.x = 0.01, .expected = 0.08079313589591118},
      {.x = 0.99, .expected = 0.08079313589591124},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, internal::BinaryEntropy(test_case.x));
  }
}

// Adapted from
// https://github.com/WICG/attribution-reporting-api/blob/ab43f8c989cf881ffd7a7f71801b98d649ed164a/flexible-event/privacy.test.ts#L10-L31
TEST(PrivacyMathTest, GetRandomizedResponseRate) {
  const struct {
    uint32_t num_states;
    double epsilon;
    double expected;
  } kTestCases[] = {{
                        .num_states = 2,
                        .epsilon = std::log(3),
                        .expected = 0.5,
                    },
                    {
                        .num_states = 3,
                        .epsilon = std::log(3),
                        .expected = 0.6,
                    },
                    {
                        .num_states = 2925,
                        .epsilon = 14,
                        .expected = 0.0024263221679834087,
                    },
                    {
                        .num_states = 3,
                        .epsilon = 14,
                        .expected = 0.000002494582008677539,
                    },
                    {
                        .num_states = 3,
                        .epsilon = 14,
                        .expected = 0.000002494582008677539,
                    },
                    {.num_states = std::numeric_limits<uint32_t>::max(),
                     .epsilon = 14,
                     .expected = 0.99972007548289821}};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, GetRandomizedResponseRate(
                                      test_case.num_states, test_case.epsilon));
  }
}

// Adapted from
// https://github.com/WICG/attribution-reporting-api/blob/ab43f8c989cf881ffd7a7f71801b98d649ed164a/flexible-event/privacy.test.ts#L38-L69
TEST(PrivacyMathTest, ComputeChannelCapacity) {
  const struct {
    uint32_t num_states;
    double epsilon;
    double expected;
  } kTestCases[] = {
      {
          .num_states = 2,
          .epsilon = std::numeric_limits<double>::infinity(),
          .expected = 1,
      },
      {
          .num_states = 1024,
          .epsilon = std::numeric_limits<double>::infinity(),
          .expected = std::log2(1024),
      },
      {
          .num_states = 3,
          .epsilon = std::numeric_limits<double>::infinity(),
          .expected = std::log2(3),
      },
      {
          .num_states = 2,
          .epsilon = std::log(3),
          .expected = 0.18872187554086717,
      },
      {
          .num_states = 2925,
          .epsilon = 14,
          .expected = 11.461727965384876,
      },
      {
          .num_states = 3,
          .epsilon = 14,
          .expected = 1.584926511508231,
      },
      {
          .num_states = 1,
          .epsilon = 14,
          .expected = 0,
      },
  };

  for (const auto& test_case : kTestCases) {
    double rate =
        GetRandomizedResponseRate(test_case.num_states, test_case.epsilon);

    EXPECT_EQ(test_case.expected,
              internal::ComputeChannelCapacity(test_case.num_states, rate));
  }
}

TEST(PrivacyMathTest, ComputeChannelCapacityWithScopes) {
  const struct {
    uint32_t num_states;
    uint32_t max_event_states;
    uint32_t attribution_scopes_limit;
    double expected;
  } kTestCases[] = {
      {
          .num_states = 2925,
          .max_event_states = 5,
          .attribution_scopes_limit = 20,
          .expected = 11.560332834212442,
      },
      {
          .num_states = 2925,
          .max_event_states = 3,
          .attribution_scopes_limit = 1,
          .expected = 11.51422090935813,
      },
      {
          .num_states = 2925,
          .max_event_states = 3,
          .attribution_scopes_limit = 5,
          .expected = 11.520127550324851,
      },
      {
          .num_states = 2925,
          .max_event_states = 165,
          .attribution_scopes_limit = 5,
          .expected = 11.807757403589267,
      },
      {
          .num_states = 300000,
          .max_event_states = 2,
          .attribution_scopes_limit = 2,
          .expected = 18.194612593092849,
      },
      {
          .num_states = 300000,
          .max_event_states = 3,
          .attribution_scopes_limit = 3,
          .expected = 18.194631828770252,
      },
      {
          .num_states = 300000,
          .max_event_states = 4,
          .attribution_scopes_limit = 4,
          .expected = 18.194660681805477,
      },
      {
          .num_states = 300000,
          .max_event_states = 5,
          .attribution_scopes_limit = 5,
          .expected = 18.194699151621514,
      },
      {
          .num_states = 1,
          .max_event_states = 5,
          .attribution_scopes_limit = 5,
          .expected = 4.3923174227787607,
      },
      // Regression test for multiplication overflow in
      // https://crbug.com/366998247
      {
          .num_states = 1,
          .max_event_states = std::numeric_limits<uint32_t>::max(),
          .attribution_scopes_limit = std::numeric_limits<uint32_t>::max(),
          .expected = 63.999999998992287,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected,
              internal::ComputeChannelCapacityScopes(
                  test_case.num_states, test_case.max_event_states,
                  test_case.attribution_scopes_limit));
  }
}

TEST(PrivacyMathTest, GetFakeReportsForSequenceIndex) {
  const struct {
    SourceType source_type;
    uint32_t sequence_index;
    std::vector<FakeEventLevelReport> expected;
  } kTestCases[] = {
      // Event sources only have 3 output states, so we can enumerate them:
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 1,
          .expected = {{.trigger_data = 0, .window_index = 0}},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 2,
          .expected = {{.trigger_data = 1, .window_index = 0}},
      },
      // Navigation sources have 2925 output states, so pick interesting ones:
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 20,
          .expected = {{.trigger_data = 3, .window_index = 0}},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 41,
          .expected =
              {
                  {.trigger_data = 4, .window_index = 0},
                  {.trigger_data = 2, .window_index = 0},
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 50,
          .expected =
              {
                  {.trigger_data = 4, .window_index = 0},
                  {.trigger_data = 4, .window_index = 0},
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 1268,
          .expected =
              {
                  {.trigger_data = 1, .window_index = 2},
                  {.trigger_data = 6, .window_index = 1},
                  {.trigger_data = 7, .window_index = 0},
              },
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected,
              internal::GetFakeReportsForSequenceIndex(
                  TriggerSpecs(test_case.source_type,
                               *EventReportWindows::FromDefaults(
                                   base::Days(30), test_case.source_type),
                               MaxEventLevelReports(test_case.source_type)),
                  test_case.sequence_index))
        << test_case.sequence_index;
  }
}

void RunRandomFakeReportsTest(const TriggerSpecs& specs,
                              const int num_samples,
                              const double tolerance) {
  std::map<std::vector<FakeEventLevelReport>, int> output_counts;
  ASSERT_OK_AND_ASSIGN(const uint32_t num_states, GetNumStates(specs));
  internal::StateMap map;
  for (int i = 0; i < num_samples; i++) {
    // Use epsilon = 0 to ensure that random data is always sampled from the RR
    // mechanism.
    ASSERT_OK_AND_ASSIGN(
        RandomizedResponseData response,
        internal::DoRandomizedResponseWithCache(
            specs,
            /*epsilon=*/0, map, SourceType::kNavigation,
            /*scopes_data=*/std::nullopt, PrivacyMathConfig()));
    ASSERT_TRUE(response.response().has_value());
    auto [it, _] =
        output_counts.try_emplace(std::move(*response.response()), 0);
    ++it->second;
  }

  // This is the coupon collector problem (see
  // https://en.wikipedia.org/wiki/Coupon_collector%27s_problem).
  // For n possible results:
  //
  // the expected number of trials needed to see all possible results is equal
  // to n * Sum_{i = 1,..,n} 1/i.
  //
  // The variance of the number of trials is equal to
  // Sum_{i = 1,.., n} (1 - p_i) / p_i^2,
  // where p_i = (n - i + 1) / n.
  //
  // The probability that t trials are not enough to see all possible results is
  // at most n^{-t/(n*ln(n)) + 1}.
  EXPECT_EQ(output_counts.size(), num_states);

  // For any of the n possible results, the expected number of times it is seen
  // is equal to 1/n. Moreover, for any possible result, the probability that it
  // is seen more than (1+alpha)*t/n times is at most p_high = exp(- D(1/n +
  // alpha/n || 1/n) * t).
  //
  // The probability that it is seen less than (1-alpha)*t/n times is at most
  // p_low = exp(-D(1/n - alpha/n || 1/n) * t,
  //
  // where D( x || y) = x * ln(x/y) + (1-x) * ln( (1-x) / (1-y) ).
  // See
  // https://en.wikipedia.org/wiki/Chernoff_bound#Additive_form_(absolute_error)
  // for details.
  //
  // Thus, the probability that the number of occurrences of one of the results
  // deviates from its expectation by alpha*t/n is at most
  // n * (p_high + p_low).
  const int expected_counts = num_samples / static_cast<double>(num_states);
  const double abs_error = expected_counts * tolerance;
  for (const auto& [_, output_count] : output_counts) {
    EXPECT_NEAR(output_count, expected_counts, abs_error);
  }
}

TEST(PrivacyMathTest, GetRandomFakeReports_Event_MatchesExpectedDistribution) {
  // The probability that not all of the 3 states are seen after `num_samples`
  // trials is at most ~1e-14476, which is 0 for all practical purposes, so the
  // `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most 1e-9.
  RunRandomFakeReportsTest(TriggerSpecs(SourceType::kEvent,
                                        *EventReportWindows::FromDefaults(
                                            base::Days(30), SourceType::kEvent),
                                        MaxEventLevelReports(1)),
                           /*num_samples=*/100'000,
                           /*tolerance=*/0.03);
}

TEST(PrivacyMathTest,
     GetRandomFakeReports_Navigation_MatchesExpectedDistribution) {
  // The probability that not all of the 2925 states are seen after
  // `num_samples` trials is at most ~1e-19, which is 0 for all practical
  // purposes, so the `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most .0002.
  RunRandomFakeReportsTest(
      TriggerSpecs(SourceType::kNavigation,
                   *EventReportWindows::FromDefaults(base::Days(30),
                                                     SourceType::kNavigation),
                   MaxEventLevelReports(3)),
      /*num_samples=*/150'000,
      /*tolerance=*/0.9);
}

TEST(PrivacyMathTest, GetRandomFakeReports_Custom_MatchesExpectedDistribution) {
  // The probability that not all of the 3 states are seen after `num_samples`
  // trials is at most ~1e-14476, which is 0 for all practical purposes, so the
  // `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most 1e-9.
  const std::vector<TriggerSpec> kSpecList = {
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(5),
          /*end_times=*/{base::Days(10), base::Days(20)})),
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(2),
          /*end_times=*/{base::Days(1)}))};

  const auto kSpecs = *TriggerSpecs::Create(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/5, /*index=*/0},
          {/*trigger_data=*/3, /*index=*/1},
          {/*trigger_data=*/4294967295, /*index=*/1},
      },
      kSpecList, MaxEventLevelReports(2));

  // The distribution check will fail with probability 6e-7.
  ASSERT_OK_AND_ASSIGN(const uint32_t num_states, GetNumStates(kSpecs));
  EXPECT_EQ(28u, num_states);
  RunRandomFakeReportsTest(kSpecs,
                           /*num_samples=*/100'000,
                           /*tolerance=*/0.1);
}

const struct {
  MaxEventLevelReports max_reports;
  std::vector<int> windows_per_type;
  base::expected<uint32_t, RandomizedResponseError> expected_num_states;
} kNumStateTestCases[] = {
    {MaxEventLevelReports(3), {3, 3, 3, 3, 3, 3, 3, 3}, 2925},
    {MaxEventLevelReports(1), {1, 1}, 3},

    {MaxEventLevelReports(1), {1}, 2},
    {MaxEventLevelReports(5), {1}, 6},
    {MaxEventLevelReports(2), {1, 1, 2, 2}, 28},
    {MaxEventLevelReports(3), {1, 1, 2, 2, 3, 3}, 455},

    // Cases for # of states > 10000 will skip the unique check, otherwise the
    // tests won't ever finish.
    {MaxEventLevelReports(20), {5, 5, 5}, 3247943160},

    // Cases that exceed `UINT32_MAX` will return a RandomizedResponseError.
    {MaxEventLevelReports(20),
     {5, 5, 5, 1},
     base::unexpected(
         RandomizedResponseError::kExceedsTriggerStateCardinalityLimit)},
};

TEST(PrivacyMathTest, GetNumStates) {
  for (const auto& test_case : kNumStateTestCases) {
    // Test both single spec and multi-spec variants to ensure both code paths
    // (optimized and non) get exercised.
    auto specs = SpecsFromWindowList(test_case.windows_per_type,
                                     /*collapse_into_single_spec=*/true,
                                     test_case.max_reports);
    EXPECT_EQ(test_case.expected_num_states, GetNumStates(specs));

    specs = SpecsFromWindowList(test_case.windows_per_type,
                                /*collapse_into_single_spec=*/false,
                                test_case.max_reports);
    EXPECT_EQ(test_case.expected_num_states, GetNumStates(specs));
  }
}

TEST(PrivacyMathTest, NumStatesForTriggerSpecs_UniqueSampling) {
  for (const auto& test_case : kNumStateTestCases) {
    auto specs = SpecsFromWindowList(test_case.windows_per_type,
                                     /*collapse_into_single_spec=*/false,
                                     test_case.max_reports);
    ASSERT_EQ(test_case.expected_num_states, GetNumStates(specs));

    if (!test_case.expected_num_states.has_value() ||
        *test_case.expected_num_states > 10000) {
      continue;
    }

    std::set<std::vector<FakeEventLevelReport>> seen_outputs;
    internal::StateMap map;
    for (uint32_t i = 0; i < *test_case.expected_num_states; i++) {
      if (auto output = internal::GetFakeReportsForSequenceIndex(specs, i, map);
          output.has_value()) {
        seen_outputs.insert(*std::move(output));
      }
    }
    EXPECT_EQ(static_cast<size_t>(*test_case.expected_num_states),
              seen_outputs.size());
  }
}

// Regression test for http://crbug.com/1503728 in which the optimized
// randomized-response incorrectly returned the trigger data *index* rather than
// the trigger data *value* in the fake reports.
TEST(PrivacyMathTest, NonDefaultTriggerDataForSingleSharedSpec) {
  // Note that the trigger data does not start at 0.
  const auto kSpecs =
      *TriggerSpecs::Create({{/*trigger_data=*/123, /*index=*/0}},
                            {TriggerSpec()}, MaxEventLevelReports(1));

  ASSERT_TRUE(kSpecs.SingleSharedSpec());

  // There are only 2 states (0 reports or 1 report with trigger data 123), so
  // loop until we hit the non-empty case.

  RandomizedResponse response;
  do {
    internal::StateMap map;

    ASSERT_OK_AND_ASSIGN(
        RandomizedResponseData response_data,
        internal::DoRandomizedResponseWithCache(
            kSpecs,
            /*epsilon=*/0, map, SourceType::kNavigation,
            /*scopes_data=*/std::nullopt, PrivacyMathConfig()));
    response = std::move(response_data.response());
  } while (!response.has_value() || response->empty());

  ASSERT_EQ(uint64_t{123u}, response->front().trigger_data);
}

TEST(PrivacyMathTest, RandomizedResponse_ExceedsChannelCapacity) {
  constexpr PrivacyMathConfig kConfig{.max_channel_capacity_navigation = 1};

  auto channel_capacity_response = DoRandomizedResponse(
      TriggerSpecs(SourceType::kNavigation, EventReportWindows(),
                   /*max_reports=*/MaxEventLevelReports(1)),
      /*epsilon=*/14, SourceType::kNavigation,
      /*scopes_data=*/std::nullopt, kConfig);

  EXPECT_THAT(channel_capacity_response,
              ErrorIs(RandomizedResponseError::kExceedsChannelCapacityLimit));
}

TEST(PrivacyMathTest, RandomizedResponse_ExceedsScopesChannelCapacity) {
  // Navigation
  auto channel_capacity_response = DoRandomizedResponse(
      TriggerSpecs(SourceType::kNavigation, EventReportWindows(),
                   /*max_reports=*/MaxEventLevelReports(1)),
      /*epsilon=*/14, SourceType::kNavigation,
      AttributionScopesData::Create(AttributionScopesSet({"1"}),
                                    /*attribution_scope_limit=*/100u,
                                    /*max_event_states=*/100u),
      PrivacyMathConfig());

  EXPECT_THAT(
      channel_capacity_response,
      ErrorIs(RandomizedResponseError::kExceedsScopesChannelCapacityLimit));

  // Event
  channel_capacity_response = DoRandomizedResponse(
      TriggerSpecs(SourceType::kEvent, EventReportWindows(),
                   /*max_reports=*/MaxEventLevelReports(1)),
      /*epsilon=*/14, SourceType::kEvent,
      AttributionScopesData::Create(AttributionScopesSet({"1"}),
                                    /*attribution_scope_limit=*/100u,
                                    /*max_event_states=*/3u),
      PrivacyMathConfig());

  EXPECT_THAT(
      channel_capacity_response,
      ErrorIs(RandomizedResponseError::kExceedsScopesChannelCapacityLimit));
}

// Regression test for http://crbug.com/1504144 in which empty specs cause an
// invalid iterator dereference and thus a crash.
TEST(PrivacyMathTest, UnaryChannel) {
  const struct {
    const char* desc;
    TriggerSpecs trigger_specs;
  } kTestCases[] = {
      {
          .desc = "empty-specs",
          .trigger_specs = *TriggerSpecs::Create(
              TriggerSpecs::TriggerDataIndices(), std::vector<TriggerSpec>(),
              MaxEventLevelReports(20)),
      },
      {
          .desc = "zero-max-reports",
          .trigger_specs =
              TriggerSpecs(SourceType::kNavigation, EventReportWindows(),
                           MaxEventLevelReports(0)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    ASSERT_OK_AND_ASSIGN(const uint32_t num_states,
                         GetNumStates(test_case.trigger_specs));
    EXPECT_EQ(1u, num_states);

    EXPECT_EQ(RandomizedResponseData(
                  /*rate=*/1,
                  /*response=*/std::vector<FakeEventLevelReport>()),
              DoRandomizedResponse(test_case.trigger_specs,
                                   /*epsilon=*/0, SourceType::kNavigation,
                                   /*scopes_data=*/std::nullopt,
                                   PrivacyMathConfig()));
  }
}

TEST(PrivacyMathTest, IsValid) {
  const TriggerSpecs kSpecs(SourceType::kNavigation,
                            *EventReportWindows::FromDefaults(
                                base::Days(30), SourceType::kNavigation),
                            MaxEventLevelReports(1));

  const struct {
    const char* desc;
    RandomizedResponse response;
    bool expected;
  } kTestCases[] = {
      {
          "null",
          std::nullopt,
          true,
      },
      {
          "non_null",
          std::vector<FakeEventLevelReport>{
              {
                  .trigger_data = 5,
                  .window_index = 1,
              },
          },
          true,
      },
      {
          "too_many_reports",
          std::vector<FakeEventLevelReport>{
              {
                  .trigger_data = 0,
                  .window_index = 0,
              },
              {
                  .trigger_data = 0,
                  .window_index = 0,
              },
          },
          false,
      },
      {
          "invalid_trigger_data",
          std::vector<FakeEventLevelReport>{
              {
                  .trigger_data = 8,
                  .window_index = 0,
              },
          },
          false,
      },
      {
          "window_index_too_large",
          std::vector<FakeEventLevelReport>{
              {
                  .trigger_data = 0,
                  .window_index = 3,
              },
          },
          false,
      },
      {
          "window_index_negative",
          std::vector<FakeEventLevelReport>{
              {
                  .trigger_data = 0,
                  .window_index = -1,
              },
          },
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    EXPECT_EQ(test_case.expected, IsValid(test_case.response, kSpecs));
  }
}

}  // namespace
}  // namespace attribution_reporting
