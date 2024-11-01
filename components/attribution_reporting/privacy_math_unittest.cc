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
using ::base::test::ValueIs;
using ::testing::UnorderedElementsAreArray;

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
          .sequence_index = 2,
          .expected = {{.trigger_data = 0, .window_index = 0}},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 1,
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
          .sequence_index = 455,
          .expected = {{.trigger_data = 3, .window_index = 0}},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 871,
          .expected =
              {
                  {.trigger_data = 4, .window_index = 0},
                  {.trigger_data = 2, .window_index = 0},
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 275,
          .expected =
              {
                  {.trigger_data = 4, .window_index = 0},
                  {.trigger_data = 4, .window_index = 0},
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 1787,
          .expected =
              {
                  {.trigger_data = 1, .window_index = 2},
                  {.trigger_data = 6, .window_index = 1},
                  {.trigger_data = 7, .window_index = 0},
              },
      },
  };

  for (int i = 0; const auto& test_case : kTestCases) {
    SCOPED_TRACE(i);

    internal::StateMap state_map;

    EXPECT_THAT(internal::GetFakeReportsForSequenceIndex(
                    TriggerSpecs(test_case.source_type,
                                 *EventReportWindows::FromDefaults(
                                     base::Days(30), test_case.source_type),
                                 MaxEventLevelReports(test_case.source_type)),
                    test_case.sequence_index, state_map),
                ValueIs(UnorderedElementsAreArray(test_case.expected)));

    ++i;
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
