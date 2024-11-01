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
      GetNumStatesRecursive(it, max_reports, num_windows, max_reports, map);

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
    uint32_t sequence_index = base::RandGenerator(num_states);
    ASSIGN_OR_RETURN(fake_reports, internal::GetFakeReportsForSequenceIndex(
                                       specs, sequence_index, map));
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
