// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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

// Let B be the trigger data cardinality.
// For every trigger data i, there are wi windows and ci maximum reports.
// Let A[C, w1, ..., wB, c1, ..., cB] be the function which counts the number
// of output states.
//
// The following helper function memoizes the recurrence relation which computes
// this:
//
// 1. A[C,w1,...,wB,c1,...,cB] = 1 if B = 0
// If there are no trigger data types to consider, there is only one possible
// output, the null output.
//
// 2. A[C,w1,...,wB,c1,...,cB] = A[C,w1,...,w{B-1},c1,...,c{B-1}] if wB = 0
// If there are no windows to consider for a particular trigger data type, then
// consider only the remaining trigger data types.
//
// 3. A[C,w1,...,wB,c1,...,cB] = sum(A[C - j,w1,...,wB - 1,c1,...,cB - j],
//                                   for j from 0 to min(c_B, C))
// Otherwise, we look at the number of possible outputs assuming we emit some
// number of reports (up to the max) for the current trigger data type under
// consideration. Given that each choice produces a distinct output, we sum
// these up.
base::CheckedNumeric<uint32_t> GetNumStatesRecursive(TriggerSpecs::Iterator it,
                                                     int max_reports,
                                                     int window_val,
                                                     int max_reports_per_type,
                                                     internal::StateMap& map) {
  // Case 1: "B = 0" there is nothing left to assign for the last data index.
  // Also consider the trivial Case 2 -> Case 1 case without touching the cache
  // or recursive calls.
  auto cur = it++;
  if (!cur || (window_val == 0 && !it)) {
    return 1;
  }

  // Store these as 8 bit to optimize storage.
  const uint8_t key[4] = {
      base::checked_cast<uint8_t>(max_reports),           //
      it.index(),                                         //
      base::checked_cast<uint8_t>(window_val),            //
      base::checked_cast<uint8_t>(max_reports_per_type),  //
  };

  uint32_t& cached = map[base::U32FromNativeEndian(key)];
  if (cached != 0) {
    return cached;
  }

  // Case 2: wB = 0.
  //
  // TODO(csharrison): Use the actual spec's max reports when that is
  // implemented. Currently we set `max_reports_per_type` to be equal to
  // `max_reports` for every type, but in the future it will be specified on the
  // `TriggerSpec` as part of the `summary_buckets` field.
  if (window_val == 0) {
    base::CheckedNumeric<uint32_t> result = GetNumStatesRecursive(
        it, max_reports, (*it).second.event_report_windows().end_times().size(),
        max_reports, map);
    std::ignore = result.AssignIfValid(&cached);
    return result;
  }

  // Case 3.
  base::CheckedNumeric<uint32_t> result = 0;
  for (int i = 0;
       result.IsValid() && i <= std::min(max_reports_per_type, max_reports);
       i++) {
    result += GetNumStatesRecursive(cur, max_reports - i, window_val - 1,
                                    max_reports_per_type - i, map);
  }
  std::ignore = result.AssignIfValid(&cached);
  return result;
}

// A variant of the above algorithm which samples a report given an index.
// This follows a similarly structured algorithm.
base::expected<void, RandomizedResponseError> GetReportsFromIndexRecursive(
    TriggerSpecs::Iterator it,
    int max_reports,
    int window_val,
    int max_reports_per_type,
    uint32_t index,
    std::vector<FakeEventLevelReport>& reports,
    internal::StateMap& map) {
  // Case 1 and Case 2 -> 1. There are no more valid trigger data values, so
  // generate nothing.
  auto cur = it++;
  if (!cur || (window_val == 0 && !it)) {
    return base::ok();
  }

  // Case 2: there are no more windows to consider for the current trigger data,
  // so generate based on the remaining trigger data types.
  //
  // TODO(csharrison): Use the actual spec's max reports when that is
  // implemented. Currently we set `max_reports_per_type` to be equal to
  // `max_reports` for every type, but in the future it will be specified on the
  // `TriggerSpec` as part of the `summary_buckets` field.
  if (window_val == 0) {
    return GetReportsFromIndexRecursive(
        it, max_reports, (*it).second.event_report_windows().end_times().size(),
        max_reports, index, reports, map);
  }

  // Case 3: For the current window and trigger data under consideration, we
  // need to choose how many reports we emit. Think of the index as pointing to
  // a particular output, where outputs are partitioned by the # of reports to
  // emit. E.g. think of each dash below as a possible output.
  //
  //       0 report              1 reports          2 reports
  // |----------------------|---------------------|-----------|
  //                        ^             ^
  //                     prev_sum       index
  //
  // The first thing we need to do is figure out how many reports to emit, this
  // is as simple as just computing the # of states with 0 reports, 1 report,
  // and so on until we find where the index slots in.
  //
  // Next, we "zoom in" to that partition of outputs in the recursive step to
  // figure out what other reports we need to emit (if any). We consider a new
  // index which just looks at the "dashes" before `index`, i.e. index' = index
  // - prev_sum.
  uint32_t prev_sum = 0;
  for (int i = 0; i <= std::min(max_reports_per_type, max_reports); i++) {
    base::CheckedNumeric<uint32_t> num_states = GetNumStatesRecursive(
        cur, max_reports - i, window_val - 1, max_reports_per_type - i, map);

    uint32_t current_sum;
    if (!base::CheckAdd(prev_sum, num_states).AssignIfValid(&current_sum)) {
      return base::unexpected(
          RandomizedResponseError::kExceedsTriggerStateCardinalityLimit);
    }

    // The index is associated with emitting `i` reports
    if (current_sum > index) {
      DCHECK_GE(index, prev_sum);

      for (int k = 0; k < i; k++) {
        reports.push_back(FakeEventLevelReport{.trigger_data = (*cur).first,
                                               .window_index = window_val - 1});
      }

      // Zoom into all other outputs that are associated with picking `i`
      // reports for this config.
      return GetReportsFromIndexRecursive(cur, max_reports - i, window_val - 1,
                                          max_reports_per_type - i,
                                          index - prev_sum, reports, map);
    }
    prev_sum = current_sum;
  }
  NOTREACHED();
}

base::expected<uint32_t, RandomizedResponseError> GetNumStatesCached(
    const TriggerSpecs& specs,
    internal::StateMap& map) {
  const int max_reports = specs.max_event_level_reports();
  if (specs.empty() || max_reports == 0) {
    return 1;
  }

  auto it = specs.begin();
  size_t num_windows = (*it).second.event_report_windows().end_times().size();

  base::CheckedNumeric<uint32_t> num_states =
      specs.SingleSharedSpec()
          ? internal::GetNumberOfStarsAndBarsSequences(  // Optimized fast path
                /*num_stars=*/static_cast<uint32_t>(max_reports),
                /*num_bars=*/static_cast<uint32_t>(specs.size() * num_windows))
          : GetNumStatesRecursive(it, max_reports, num_windows, max_reports,
                                  map);

  if (!num_states.IsValid() ||
      num_states.ValueOrDie() > g_max_trigger_state_cardinality) {
    return base::unexpected(
        RandomizedResponseError::kExceedsTriggerStateCardinalityLimit);
  }
  return num_states.ValueOrDie();
}

}  // namespace

RandomizedResponseData::RandomizedResponseData(double rate,
                                               RandomizedResponse response)
    : rate_(rate),
      response_(std::move(response)) {
  DCHECK_GE(rate_, 0);
  DCHECK_LE(rate_, 1);
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
  DCHECK_GE(r, 0);
  DCHECK_LE(r, 1);
  return r > 0 && (r == 1 || base::RandDouble() < r);
}

double GetRandomizedResponseRate(uint32_t num_states, double epsilon) {
  DCHECK_GT(num_states, 0u);

  return num_states / (num_states - 1.0 + std::exp(epsilon));
}

base::expected<uint32_t, RandomizedResponseError> GetNumStates(
    const TriggerSpecs& specs) {
  internal::StateMap map;
  return GetNumStatesCached(specs, map);
}

base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponse(const TriggerSpecs& specs,
                     double epsilon,
                     mojom::SourceType source_type,
                     const std::optional<AttributionScopesData>& scopes_data,
                     const PrivacyMathConfig& config) {
  internal::StateMap map;
  return internal::DoRandomizedResponseWithCache(
      specs, epsilon, map, source_type, scopes_data, config);
}

bool IsValid(const RandomizedResponse& response, const TriggerSpecs& specs) {
  if (!response.has_value()) {
    return true;
  }

  return base::MakeStrictNum(response->size()) <=
             static_cast<int>(specs.max_event_level_reports()) &&
         base::ranges::all_of(*response, [&](const FakeEventLevelReport&
                                                 report) {
           const auto spec = specs.find(report.trigger_data,
                                        mojom::TriggerDataMatching::kExact);
           return spec != specs.end() && report.window_index >= 0 &&
                  base::MakeStrictNum(report.window_index) <
                      (*spec).second.event_report_windows().end_times().size();
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

base::expected<std::vector<FakeEventLevelReport>, RandomizedResponseError>
GetFakeReportsForSequenceIndex(const TriggerSpecs& specs,
                               base::StrictNumeric<uint32_t> index,
                               StateMap& map) {
  std::vector<FakeEventLevelReport> reports;

  const int max_reports = specs.max_event_level_reports();
  if (specs.empty() || max_reports == 0) {
    return reports;
  }

  auto it = specs.begin();
  RETURN_IF_ERROR(GetReportsFromIndexRecursive(
      it, max_reports, (*it).second.event_report_windows().end_times().size(),
      max_reports, index, reports, map));
  return reports;
}

base::CheckedNumeric<uint32_t> GetNumberOfStarsAndBarsSequences(
    base::StrictNumeric<uint32_t> num_stars,
    base::StrictNumeric<uint32_t> num_bars) {
  return BinomialCoefficient(
      static_cast<uint32_t>(num_stars) + static_cast<uint32_t>(num_bars),
      num_stars);
}

base::expected<std::vector<uint32_t>, absl::monostate> GetStarIndices(
    base::StrictNumeric<uint32_t> num_stars,
    base::StrictNumeric<uint32_t> num_bars,
    base::StrictNumeric<uint32_t> sequence_index) {
  const base::CheckedNumeric<uint32_t> num_sequences =
      GetNumberOfStarsAndBarsSequences(num_stars, num_bars);
  if (!num_sequences.IsValid()) {
    return base::unexpected(absl::monostate());
  }

  DCHECK(sequence_index < num_sequences.ValueOrDie());
  return GetKCombinationAtIndex(sequence_index, num_stars);
}

std::vector<uint32_t> GetBarsPrecedingEachStar(std::vector<uint32_t> out) {
  DCHECK(base::ranges::is_sorted(out, std::greater{}));

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
  DCHECK_GT(num_states, 0u);
  DCHECK_GE(randomized_response_rate, 0);
  DCHECK_LE(randomized_response_rate, 1);

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
    const TriggerSpecs& specs,
    base::StrictNumeric<uint32_t> random_stars_and_bars_sequence_index) {
  const TriggerSpec* single_spec = specs.SingleSharedSpec();
  CHECK(single_spec);

  const int trigger_data_cardinality = specs.size();
  const int max_reports = specs.max_event_level_reports();

  ASSIGN_OR_RETURN(
      std::vector<uint32_t> stars,
      GetStarIndices(
          /*num_stars=*/static_cast<uint32_t>(max_reports),
          /*num_bars=*/
          static_cast<uint32_t>(
              trigger_data_cardinality *
              single_spec->event_report_windows().end_times().size()),
          /*sequence_index=*/random_stars_and_bars_sequence_index),
      [](absl::monostate) {
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
    DCHECK_LT(trigger_data_index, trigger_data_cardinality);

    fake_reports.push_back({
        .trigger_data =
            std::next(specs.trigger_data_indices().begin(), trigger_data_index)
                ->first,
        .window_index = result.quot,
    });
  }
  DCHECK_LE(fake_reports.size(), static_cast<size_t>(max_reports));
  return fake_reports;
}

base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponseWithCache(
    const TriggerSpecs& specs,
    double epsilon,
    StateMap& map,
    mojom::SourceType source_type,
    const std::optional<AttributionScopesData>& scopes_data,
    const PrivacyMathConfig& config) {
  ASSIGN_OR_RETURN(const uint32_t num_states, GetNumStatesCached(specs, map));

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
    // TODO(csharrison): Justify the fast path with `single_spec` with
    // profiling.
    //
    // Note: we can implement the fast path in more cases than a single shared
    // spec if all of the specs have the same # of windows and reports. We can
    // consider further optimizing if it's useful. The existing code will cover
    // the default specs for navigation / event sources.
    uint32_t sequence_index = base::RandGenerator(num_states);
    if (specs.SingleSharedSpec()) {
      ASSIGN_OR_RETURN(fake_reports, internal::GetFakeReportsForSequenceIndex(
                                         specs, sequence_index));
    } else {
      ASSIGN_OR_RETURN(fake_reports, internal::GetFakeReportsForSequenceIndex(
                                         specs, sequence_index, map));
    }
  }
  return RandomizedResponseData(rate, std::move(fake_reports));
}

}  // namespace internal

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
