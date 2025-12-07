// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_config.h"

namespace attribution_reporting {

namespace {

// Although the theoretical maximum number of trigger states exceeds 32 bits,
// we've chosen to only support a maximal trigger state cardinality of
// `UINT32_MAX` due to the randomized response generation rate being close
// enough to 1 for that number of states to not warrant the extra cost in
// resources for larger ints. The arithmetic in this file mostly adheres to that
// by way of overflow checking, with only certain exceptions applying. If the
// max trigger state cardinality is ever increased, the typings in this file
// must be changed to support that.

// Controls the max number of report states allowed for a given source
// registration.
uint32_t g_max_trigger_state_cardinality = std::numeric_limits<uint32_t>::max();

}  // namespace

base::expected<uint32_t, RandomizedResponseError> GetNumStates(
    const TriggerDataSet& trigger_data,
    const EventReportWindows& event_report_windows,
    const MaxEventLevelReports max_event_level_reports) {
  const int max_reports = max_event_level_reports;
  if (trigger_data.trigger_data().empty() || max_reports == 0) {
    return 1;
  }

  size_t num_windows = event_report_windows.end_times().size();

  base::CheckedNumeric<uint32_t> num_states =
      internal::GetNumberOfStarsAndBarsSequences(
          /*num_stars=*/static_cast<uint32_t>(max_reports),
          /*num_bars=*/static_cast<uint32_t>(
              trigger_data.trigger_data().size() * num_windows));

  if (!num_states.IsValid() ||
      num_states.ValueOrDie() > g_max_trigger_state_cardinality) {
    return base::unexpected(
        RandomizedResponseError::kExceedsTriggerStateCardinalityLimit);
  }
  return num_states.ValueOrDie();
}

RandomizedResponseData::RandomizedResponseData(double rate,
                                               RandomizedResponse response)
    : rate_(rate), response_(std::move(response)) {
  CHECK_GE(rate_, 0);
  CHECK_LE(rate_, 1);
}

RandomizedResponseData::~RandomizedResponseData() = default;

RandomizedResponseData::RandomizedResponseData(const RandomizedResponseData&) =
    default;

RandomizedResponseData& RandomizedResponseData::operator=(
    const RandomizedResponseData&) = default;

RandomizedResponseData::RandomizedResponseData(RandomizedResponseData&&) =
    default;

RandomizedResponseData& RandomizedResponseData::operator=(
    RandomizedResponseData&&) = default;

uint32_t MaxTriggerStateCardinality() {
  return g_max_trigger_state_cardinality;
}

double PrivacyMathConfig::GetMaxChannelCapacity(
    mojom::SourceType source_type) const {
  switch (source_type) {
    case mojom::SourceType::kNavigation:
      return max_channel_capacity_navigation;
    case mojom::SourceType::kEvent:
      return max_channel_capacity_event;
  }
  NOTREACHED();
}

double PrivacyMathConfig::GetMaxChannelCapacityScopes(
    mojom::SourceType source_type) const {
  switch (source_type) {
    case mojom::SourceType::kNavigation:
      return max_channel_capacity_scopes_navigation;
    case mojom::SourceType::kEvent:
      return max_channel_capacity_scopes_event;
  }
  NOTREACHED();
}

bool GenerateWithRate(double r) {
  CHECK_GE(r, 0);
  CHECK_LE(r, 1);
  return r > 0 && (r == 1 || base::RandDouble() < r);
}

double GetRandomizedResponseRate(uint32_t num_states, double epsilon) {
  CHECK_GT(num_states, 0u);

  return num_states / (num_states - 1.0 + std::exp(epsilon));
}

bool IsValid(const RandomizedResponse& response,
             const TriggerDataSet& trigger_data,
             const EventReportWindows& event_report_windows,
             MaxEventLevelReports max_event_level_reports) {
  if (!response.has_value()) {
    return true;
  }

  return base::MakeStrictNum(response->size()) <=
             static_cast<int>(max_event_level_reports) &&
         std::ranges::all_of(
             *response, [&](const FakeEventLevelReport& report) {
               const bool has_trigger_data =
                   trigger_data.trigger_data().contains(report.trigger_data);

               return has_trigger_data && report.window_index >= 0 &&
                      base::MakeStrictNum(report.window_index) <
                          event_report_windows.end_times().size();
             });
}

namespace internal {

base::CheckedNumeric<uint32_t> BinomialCoefficient(
    base::StrictNumeric<uint32_t> strict_n,
    base::StrictNumeric<uint32_t> strict_k) {
  uint32_t n = strict_n;
  uint32_t k = strict_k;
  if (k > n) {
    return 0;
  }

  // Speed up some trivial cases.
  if (k == n || n == 0) {
    return 1;
  }

  // BinomialCoefficient(n, k) == BinomialCoefficient(n, n - k),
  // So simplify if possible. Underflow not possible as we know k < n at this
  // point.
  if (k > n - k) {
    k = n - k;
  }

  // (n choose k) = n (n -1) ... (n - (k - 1)) / k!
  // = mul((n + 1 - i) / i), i from 1 -> k.
  //
  // You might be surprised that this algorithm works just fine with integer
  // division (i.e. division occurs cleanly with no remainder). However, this is
  // true for a very simple reason. Imagine a value of `i` causes division with
  // remainder in the below algorithm. This immediately implies that
  // (n choose i) is fractional, which we know is not the case.
  base::CheckedNumeric<uint64_t> result = 1;
  for (uint32_t i = 1; i <= k; i++) {
    uint32_t term = n - i + 1;
    base::CheckedNumeric<uint64_t> temp_result = result * term;
    DCHECK(!temp_result.IsValid() || (temp_result % i).ValueOrDie() == 0);
    result = temp_result / i;
  }
  return result.Cast<uint32_t>();
}

// Computes the `combination_index`-th lexicographically smallest k-combination.
// https://en.wikipedia.org/wiki/Combinatorial_number_system

// A k-combination is a sequence of k non-negative integers in decreasing order.
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0.
// k-combinations can be ordered lexicographically, with the smallest
// k-combination being a_k=k-1, a_{k-1}=k-2, .., a_1=0. Given an index
// `combination_index`>=0, and an order k, this method returns the
// `combination_index`-th smallest k-combination.
//
// Given an index `combination_index`, the `combination_index`-th k-combination
// is the unique set of k non-negative integers
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0
// such that `combination_index` = \sum_{i=1}^k {a_i}\choose{i}
//
// For k >= 2, we find this set via a simple greedy algorithm.
// http://math0.wvstateu.edu/~baker/cs405/code/Combinadics.html
//
// The k = 0 case is trivially the empty set, and the k = 1 case is
// trivially just `combination_index`.
std::vector<uint32_t> GetKCombinationAtIndex(
    base::StrictNumeric<uint32_t> combination_index,
    base::StrictNumeric<uint32_t> strict_k) {
  uint32_t k = strict_k;
  DCHECK_LE(k, kMaxSettableEventLevelAttributionsPerSource);

  std::vector<uint32_t> output_k_combination;
  output_k_combination.reserve(k);

  if (k == 0u) {
    return output_k_combination;
  }

  if (k == 1u) {
    output_k_combination.push_back(combination_index);
    return output_k_combination;
  }

  // To find a_k, iterate candidates upwards from 0 until we've found the
  // maximum a such that (a choose k) <= `combination_index`. Let a_k = a. Use
  // the previous binomial coefficient to compute the next one. Note: possible
  // to speed this up via something other than incremental search.
  uint32_t target = combination_index;

  uint32_t candidate = k - 1;

  // BinomialCoefficient(candidate, k)
  uint64_t binomial_coefficient = 0;
  // BinomialCoefficient(candidate+1, k)
  uint64_t next_binomial_coefficient = 1;
  while (next_binomial_coefficient <= target) {
    DCHECK_LT(candidate, std::numeric_limits<uint32_t>::max());
    candidate++;
    binomial_coefficient = next_binomial_coefficient;

    // If the returned value from `BinomialCoefficient` is invalid, the DCHECK
    // would fail anyways, so it is safe to not validate.
    DCHECK(binomial_coefficient ==
           BinomialCoefficient(candidate, k).ValueOrDie());

    // (n + 1 choose k) = (n choose k) * (n + 1) / (n + 1 - k)
    // Safe because candidate <= binomial_coefficient <= UINT32_MAX.
    // Therefore binomial_coefficient * (candidate + 1) <= UINT32_MAX *
    // (UINT32_MAX + 1) <= UINT64_MAX.
    next_binomial_coefficient = binomial_coefficient * (candidate + 1);
    next_binomial_coefficient /= candidate + 1 - k;
  }
  // We know from the k-combination definition, all subsequent values will be
  // strictly decreasing. Find them all by decrementing `candidate`.
  // Use the previous binomial coefficient to compute the next one.
  uint32_t current_k = k;
  while (true) {
    // The optimized code below maintains this loop invariant.
    DCHECK(binomial_coefficient ==
           BinomialCoefficient(candidate, current_k).ValueOrDie());

    if (binomial_coefficient <= target) {
      output_k_combination.push_back(candidate);
      bool valid =
          base::CheckSub(target, binomial_coefficient).AssignIfValid(&target);
      DCHECK(valid);

      if (output_k_combination.size() == static_cast<size_t>(k)) {
        DCHECK_EQ(target, 0u);
        return output_k_combination;
      }
      // (n - 1 choose k - 1) = (n choose k) * k / n
      // Safe because binomial_coefficient * current_k <= combination_index * k
      // <= UINT32_MAX * UINT32_MAX < UINT64_MAX.
      binomial_coefficient = binomial_coefficient * current_k / candidate;

      current_k--;
      candidate--;
    } else {
      // (n - 1 choose k) = (n choose k) * (n - k) / n
      // Safe because binomial_coefficient * (candidate - current_k) <=
      // combination_index * k <= UINT32_MAX * UINT32_MAX < UINT64_MAX.
      binomial_coefficient =
          binomial_coefficient * (candidate - current_k) / candidate;

      candidate--;
    }
    DCHECK(base::IsValueInRangeForNumericType<uint32_t>(binomial_coefficient));
  }
}

base::CheckedNumeric<uint32_t> GetNumberOfStarsAndBarsSequences(
    base::StrictNumeric<uint32_t> num_stars,
    base::StrictNumeric<uint32_t> num_bars) {
  return BinomialCoefficient(
      static_cast<uint32_t>(num_stars) + static_cast<uint32_t>(num_bars),
      num_stars);
}

base::expected<std::vector<uint32_t>, std::monostate> GetStarIndices(
    base::StrictNumeric<uint32_t> num_stars,
    base::StrictNumeric<uint32_t> num_bars,
    base::StrictNumeric<uint32_t> sequence_index) {
  const base::CheckedNumeric<uint32_t> num_sequences =
      GetNumberOfStarsAndBarsSequences(num_stars, num_bars);
  if (!num_sequences.IsValid()) {
    return base::unexpected(std::monostate());
  }

  DCHECK(sequence_index < num_sequences.ValueOrDie());
  return GetKCombinationAtIndex(sequence_index, num_stars);
}

std::vector<uint32_t> GetBarsPrecedingEachStar(std::vector<uint32_t> out) {
  DCHECK(std::ranges::is_sorted(out, std::greater{}));

  for (size_t i = 0u; i < out.size(); i++) {
    uint32_t star_index = out[i];

    // There are `star_index` prior positions in the sequence, and `i` prior
    // stars, so there are `star_index` - `i` prior bars.
    out[i] = star_index - (out.size() - 1 - i);
  }
  return out;
}

double BinaryEntropy(double p) {
  if (p == 0 || p == 1) {
    return 0;
  }

  return -p * log2(p) - (1 - p) * log2(1 - p);
}

double ComputeChannelCapacity(
    const base::StrictNumeric<uint32_t> num_states_strict,
    const double randomized_response_rate) {
  uint32_t num_states = num_states_strict;
  CHECK_GT(num_states, 0u);
  CHECK_GE(randomized_response_rate, 0);
  CHECK_LE(randomized_response_rate, 1);

  // The capacity of a unary channel is 0. This follows from the definition
  // of mutual information.
  if (num_states == 1u || randomized_response_rate == 1) {
    return 0;
  }

  double num_states_double = static_cast<double>(num_states);
  double p =
      randomized_response_rate * (num_states_double - 1) / num_states_double;
  return log2(num_states_double) - BinaryEntropy(p) -
         p * log2(num_states_double - 1);
}

double ComputeChannelCapacityScopes(
    const base::StrictNumeric<uint32_t> num_states,
    const base::StrictNumeric<uint32_t> max_event_states,
    const base::StrictNumeric<uint32_t> attribution_scope_limit) {
  CHECK(num_states > 0u);
  CHECK(attribution_scope_limit > 0u);

  // Ensure that `double` arithmetic is performed here instead of `uint32_t`,
  // which can overflow and produce incorrect results, e.g.
  // https://crbug.com/366998247.
  double total_states = static_cast<double>(num_states) +
                        static_cast<double>(max_event_states) *
                            (static_cast<double>(attribution_scope_limit) - 1);

  return log2(total_states);
}

base::expected<std::vector<FakeEventLevelReport>, RandomizedResponseError>
GetFakeReportsForSequenceIndex(
    const TriggerDataSet& trigger_data,
    const EventReportWindows& event_report_windows,
    const MaxEventLevelReports max_event_level_reports,
    base::StrictNumeric<uint32_t> random_stars_and_bars_sequence_index) {
  const int trigger_data_cardinality = trigger_data.trigger_data().size();
  const int max_reports = max_event_level_reports;

  ASSIGN_OR_RETURN(
      std::vector<uint32_t> stars,
      GetStarIndices(
          /*num_stars=*/static_cast<uint32_t>(max_reports),
          /*num_bars=*/
          static_cast<uint32_t>(trigger_data_cardinality *
                                event_report_windows.end_times().size()),
          /*sequence_index=*/random_stars_and_bars_sequence_index),
      [](std::monostate) {
        return RandomizedResponseError::kExceedsTriggerStateCardinalityLimit;
      });

  const std::vector<uint32_t> bars_preceding_each_star =
      GetBarsPrecedingEachStar(std::move(stars));

  std::vector<FakeEventLevelReport> fake_reports;

  // an output state is uniquely determined by an ordering of c stars and w*d
  // bars, where:
  // w = the number of reporting windows
  // c = the maximum number of reports for a source
  // d = the trigger data cardinality for a source
  for (uint32_t num_bars : bars_preceding_each_star) {
    if (num_bars == 0) {
      continue;
    }

    auto result = std::div(num_bars - 1, trigger_data_cardinality);

    const int trigger_data_index = result.rem;
    CHECK_LT(trigger_data_index, trigger_data_cardinality);

    fake_reports.push_back({
        .trigger_data =
            *std::next(trigger_data.trigger_data().begin(), trigger_data_index),
        .window_index = result.quot,
    });
  }
  DCHECK_LE(fake_reports.size(), static_cast<size_t>(max_reports));
  return fake_reports;
}

}  // namespace internal

base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponse(const TriggerDataSet& trigger_data,
                     const EventReportWindows& event_report_windows,
                     const MaxEventLevelReports max_event_level_reports,
                     double epsilon,
                     mojom::SourceType source_type,
                     const std::optional<AttributionScopesData>& scopes_data,
                     const PrivacyMathConfig& config) {
  ASSIGN_OR_RETURN(const uint32_t num_states,
                   GetNumStates(trigger_data, event_report_windows,
                                max_event_level_reports));
  base::UmaHistogramCounts100000("Conversions.NumTriggerStates",
                                 base::ClampedNumeric(num_states));

  double rate = GetRandomizedResponseRate(num_states, epsilon);
  double channel_capacity = internal::ComputeChannelCapacity(num_states, rate);
  if (channel_capacity > config.GetMaxChannelCapacity(source_type)) {
    return base::unexpected(
        RandomizedResponseError::kExceedsChannelCapacityLimit);
  }

  if (scopes_data.has_value()) {
    if (source_type == mojom::SourceType::kEvent &&
        num_states > scopes_data->max_event_states()) {
      return base::unexpected(
          RandomizedResponseError::kExceedsMaxEventStatesLimit);
    }

    double scopes_channel_capacity = internal::ComputeChannelCapacityScopes(
        num_states, scopes_data->max_event_states(),
        scopes_data->attribution_scope_limit());
    if (scopes_channel_capacity >
        config.GetMaxChannelCapacityScopes(source_type)) {
      return base::unexpected(
          RandomizedResponseError::kExceedsScopesChannelCapacityLimit);
    }
  }

  std::optional<std::vector<FakeEventLevelReport>> fake_reports;
  if (GenerateWithRate(rate)) {
    uint32_t sequence_index = base::RandGenerator(num_states);
    ASSIGN_OR_RETURN(fake_reports,
                     internal::GetFakeReportsForSequenceIndex(
                         trigger_data, event_report_windows,
                         max_event_level_reports, sequence_index));
  }
  return RandomizedResponseData(rate, std::move(fake_reports));
}

ScopedMaxTriggerStateCardinalityForTesting::
    ScopedMaxTriggerStateCardinalityForTesting(
        uint32_t max_trigger_state_cardinality)
    : previous_(g_max_trigger_state_cardinality) {
  CHECK_GT(max_trigger_state_cardinality, 0u);
  g_max_trigger_state_cardinality = max_trigger_state_cardinality;
}

ScopedMaxTriggerStateCardinalityForTesting::
    ~ScopedMaxTriggerStateCardinalityForTesting() {
  g_max_trigger_state_cardinality = previous_;
}

}  // namespace attribution_reporting
