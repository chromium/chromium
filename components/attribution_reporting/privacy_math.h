// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_

#include <stdint.h>

#include <compare>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"

namespace attribution_reporting {

class AttributionScopesData;
class TriggerSpecs;

struct FakeEventLevelReport {
  uint32_t trigger_data;
  int window_index;

  friend std::strong_ordering operator<=>(const FakeEventLevelReport&,
                                          const FakeEventLevelReport&) =
      default;
};

// Corresponds to `StoredSource::AttributionLogic` as follows:
// `std::nullopt` -> `StoredSource::AttributionLogic::kTruthfully`
// empty vector -> `StoredSource::AttributionLogic::kNever`
// non-empty vector -> `StoredSource::AttributionLogic::kFalsely`
using RandomizedResponse = std::optional<std::vector<FakeEventLevelReport>>;

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool IsValid(const RandomizedResponse&, const TriggerSpecs&);

enum class RandomizedResponseError {
  kExceedsChannelCapacityLimit,
  kExceedsScopesChannelCapacityLimit,
  kExceedsTriggerStateCardinalityLimit,
  kExceedsMaxEventStatesLimit,
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) RandomizedResponseData {
 public:
  RandomizedResponseData(double rate,
                         RandomizedResponse);

  ~RandomizedResponseData();

  RandomizedResponseData(const RandomizedResponseData&);
  RandomizedResponseData& operator=(const RandomizedResponseData&);

  RandomizedResponseData(RandomizedResponseData&&);
  RandomizedResponseData& operator=(RandomizedResponseData&&);

  double rate() const { return rate_; }

  const RandomizedResponse& response() const { return response_; }

  RandomizedResponse& response() { return response_; }

  friend bool operator==(const RandomizedResponseData&,
                         const RandomizedResponseData&) = default;

 private:
  double rate_;
  RandomizedResponse response_;
};

// Returns true with probability r.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING) bool GenerateWithRate(double r);

// https://wicg.github.io/attribution-reporting-api/#obtain-a-randomized-source-response-pick-rate
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
double GetRandomizedResponseRate(uint32_t num_states, double epsilon);

// Returns the number of possible output states for the given API configuration.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<uint32_t, RandomizedResponseError> GetNumStates(
    const TriggerSpecs& specs);

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) PrivacyMathConfig {
  // Controls the max number bits of information that can be associated with
  // a single source.
  double max_channel_capacity_navigation = 11.5;
  double max_channel_capacity_scopes_navigation = 11.55;
  double max_channel_capacity_event = 6.5;
  double max_channel_capacity_scopes_event = 6.5;

  double GetMaxChannelCapacity(mojom::SourceType) const;
  double GetMaxChannelCapacityScopes(mojom::SourceType) const;

  friend bool operator==(const PrivacyMathConfig&,
                         const PrivacyMathConfig&) = default;
};

// Determines the randomized response flip probability for the given API
// configuration, and performs randomized response on that output space.
//
// Returns `std::nullopt` if the output should be determined truthfully.
// Otherwise will return a vector of fake reports.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponse(const TriggerSpecs& specs,
                     double epsilon,
                     mojom::SourceType,
                     const std::optional<AttributionScopesData>&,
                     const PrivacyMathConfig&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING) uint32_t MaxTriggerStateCardinality();

// Exposed for testing purposes.
namespace internal {

// Computes the binary entropy function:
// https://en.wikipedia.org/wiki/Binary_entropy_function
COMPONENT_EXPORT(ATTRIBUTION_REPORTING) double BinaryEntropy(double p);

// Computes the channel capacity of a qary-symmetric channel.
// https://wicg.github.io/attribution-reporting-api/#computing-channel-capacity
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
double ComputeChannelCapacity(base::StrictNumeric<uint32_t> num_states_strict,
                              double randomized_response_rate);

// Computes the upper-bound channel capacity of a qary-symmetric channel for a
// given attribution scopes configuration.
// https://wicg.github.io/attribution-reporting-api/#compute-the-scopes-channel-capacity-of-a-source
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
double ComputeChannelCapacityScopes(
    base::StrictNumeric<uint32_t> num_states_strict,
    base::StrictNumeric<uint32_t> max_event_states,
    base::StrictNumeric<uint32_t> attribution_scope_limit);

// Takes a `StateMap`, to optimize with the cache from previous calls that
// pre-compute the number of states (`GetNumStatesRecursive()`).
using StateMap = std::map<uint32_t, uint32_t>;
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::vector<FakeEventLevelReport>, RandomizedResponseError>
GetFakeReportsForSequenceIndex(const TriggerSpecs& specs,
                               base::StrictNumeric<uint32_t> index,
                               StateMap& map);

// Exposed to speed up tests which perform randomized response many times in a
// row.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponseWithCache(
    const TriggerSpecs& specs,
    double epsilon,
    StateMap& map,
    mojom::SourceType,
    const std::optional<AttributionScopesData>& scopes_data,
    const PrivacyMathConfig&);

}  // namespace internal

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    ScopedMaxTriggerStateCardinalityForTesting {
 public:
  explicit ScopedMaxTriggerStateCardinalityForTesting(uint32_t);

  ~ScopedMaxTriggerStateCardinalityForTesting();

  ScopedMaxTriggerStateCardinalityForTesting(
      const ScopedMaxTriggerStateCardinalityForTesting&) = delete;
  ScopedMaxTriggerStateCardinalityForTesting& operator=(
      const ScopedMaxTriggerStateCardinalityForTesting&) = delete;

  ScopedMaxTriggerStateCardinalityForTesting(
      ScopedMaxTriggerStateCardinalityForTesting&&) = delete;
  ScopedMaxTriggerStateCardinalityForTesting& operator=(
      ScopedMaxTriggerStateCardinalityForTesting&&) = delete;

 private:
  uint32_t previous_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_
