// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_util.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/prefs/retention_state_snapshot.h"

namespace multistep_filter {

namespace {

using StringToTaskTypeMap =
    base::fixed_flat_map<std::string_view,
                         MultistepFilterTaskType,
                         static_cast<size_t>(
                             MultistepFilterTaskType::kMaxValue)>;
using StringToFacetTypeMap =
    base::fixed_flat_map<std::string_view,
                         MultistepFilterFacetType,
                         static_cast<size_t>(
                             MultistepFilterFacetType::kMaxValue)>;

constexpr StringToTaskTypeMap kStringToTaskType =
    base::MakeFixedFlatMap<std::string_view, MultistepFilterTaskType>({
        {kMultistepFilterTaskTypeSearchFlights,
         MultistepFilterTaskType::kSearchFlights},
        {kMultistepFilterTaskTypeSearchAccommodations,
         MultistepFilterTaskType::kSearchAccommodations},
    });

static_assert(
    kStringToTaskType.size() ==
        static_cast<size_t>(MultistepFilterTaskType::kMaxValue),
    "kStringToTaskType size must match MultistepFilterTaskType::kMaxValue");

constexpr StringToFacetTypeMap kStringToFacetType =
    base::MakeFixedFlatMap<std::string_view, MultistepFilterFacetType>({
        {kMultistepFilterFacetTypeAgeChild,
         MultistepFilterFacetType::kAgeChild},
        {kMultistepFilterFacetTypeAmenityFreeBreakfast,
         MultistepFilterFacetType::kAmenityFreeBreakfast},
        {kMultistepFilterFacetTypeAmenityFreeWifi,
         MultistepFilterFacetType::kAmenityFreeWifi},
        {kMultistepFilterFacetTypeCabinClass,
         MultistepFilterFacetType::kCabinClass},
        {kMultistepFilterFacetTypeCountAdult,
         MultistepFilterFacetType::kCountAdult},
        {kMultistepFilterFacetTypeCountChild,
         MultistepFilterFacetType::kCountChild},
        {kMultistepFilterFacetTypeCountInfant,
         MultistepFilterFacetType::kCountInfant},
        {kMultistepFilterFacetTypeCountInfantInLap,
         MultistepFilterFacetType::kCountInfantInLap},
        {kMultistepFilterFacetTypeCountInfantInSeat,
         MultistepFilterFacetType::kCountInfantInSeat},
        {kMultistepFilterFacetTypeCountRoom,
         MultistepFilterFacetType::kCountRoom},
        {kMultistepFilterFacetTypeDateCheckin,
         MultistepFilterFacetType::kDateCheckin},
        {kMultistepFilterFacetTypeDateCheckout,
         MultistepFilterFacetType::kDateCheckout},
        {kMultistepFilterFacetTypeDateInbound,
         MultistepFilterFacetType::kDateInbound},
        {kMultistepFilterFacetTypeDateOutbound,
         MultistepFilterFacetType::kDateOutbound},
        {kMultistepFilterFacetTypeLocationDestination,
         MultistepFilterFacetType::kLocationDestination},
        {kMultistepFilterFacetTypeLocationInbound,
         MultistepFilterFacetType::kLocationInbound},
        {kMultistepFilterFacetTypeLocationOutbound,
         MultistepFilterFacetType::kLocationOutbound},
        {kMultistepFilterFacetTypePolicyFreeCancellation,
         MultistepFilterFacetType::kPolicyFreeCancellation},
        {kMultistepFilterFacetTypePolicyPetsAllowed,
         MultistepFilterFacetType::kPolicyPetsAllowed},
        {kMultistepFilterFacetTypeRatingReviewMin,
         MultistepFilterFacetType::kRatingReviewMin},
        {kMultistepFilterFacetTypeRatingStarMin,
         MultistepFilterFacetType::kRatingStarMin},
        {kMultistepFilterFacetTypeStops, MultistepFilterFacetType::kStops},
        {kMultistepFilterFacetTypeTripType,
         MultistepFilterFacetType::kTripType},
    });

static_assert(
    kStringToFacetType.size() ==
        static_cast<size_t>(MultistepFilterFacetType::kMaxValue),
    "kStringToFacetType size must match MultistepFilterFacetType::kMaxValue");

}  // namespace

bool IsSameEtldPlusOne(const UrlFilterSuggestion& suggestion) {
  return GetEtldPlusOneForHost(base::UTF16ToUTF8(suggestion.source_host)) ==
         GetEtldPlusOneForHost(suggestion.triggering_host);
}

base::TimeDelta GetClampedDifference(base::TimeTicks end,
                                     base::TimeTicks start) {
  if (end.is_null() || start.is_null() || end < start) {
    return base::TimeDelta();
  }
  return end - start;
}

base::TimeDelta GetClampedDifference(base::Time end, base::Time start) {
  if (end.is_null() || start.is_null() || end < start) {
    return base::TimeDelta();
  }
  return end - start;
}

MultistepFilterTaskType MapStringToTaskType(std::string_view task_type) {
  StringToTaskTypeMap::const_iterator it = kStringToTaskType.find(task_type);
  return it != kStringToTaskType.end() ? it->second
                                       : MultistepFilterTaskType::kUnknown;
}

MultistepFilterFacetType MapStringToFacetType(std::string_view filter_facet) {
  StringToFacetTypeMap::const_iterator it =
      kStringToFacetType.find(filter_facet);
  return it != kStringToFacetType.end() ? it->second
                                        : MultistepFilterFacetType::kUnknown;
}

MultistepFilterRetentionState GetRetentionState(
    const RetentionStateSnapshot& snapshot) {
  if (snapshot.suggestion_impressions == 0) {
    return MultistepFilterRetentionState::kFirstImpression;
  }
  if (snapshot.is_last_suggestion_accepted) {
    return MultistepFilterRetentionState::kAcceptedLastTime;
  }
  if (snapshot.suggestion_acceptances > 0) {
    return MultistepFilterRetentionState::kRejectedLastTime_AcceptedAtLeastOnce;
  }
  return MultistepFilterRetentionState::kRejectedLastTime_NeverAccepted;
}

void EnumerateActiveRetentionSlices(
    MultistepFilterRetentionState state,
    base::FunctionRef<void(std::string_view)> callback) {
  switch (state) {
    case MultistepFilterRetentionState::kFirstImpression:
      callback(kRetentionSliceFirstImpression);
      break;
    case MultistepFilterRetentionState::kAcceptedLastTime:
      callback(kRetentionSliceAcceptedLastTime);
      callback(kRetentionSliceAcceptedAtLeastOnce);
      break;
    case MultistepFilterRetentionState::kRejectedLastTime_AcceptedAtLeastOnce:
      callback(kRetentionSliceRejectedLastTime);
      callback(kRetentionSliceAcceptedAtLeastOnce);
      break;
    case MultistepFilterRetentionState::kRejectedLastTime_NeverAccepted:
      callback(kRetentionSliceRejectedLastTime);
      callback(kRetentionSliceSawCuesButNeverAccepted);
      break;
  }
}

}  // namespace multistep_filter
