// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <utility>

#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

TriggerRegistration::TriggerRegistration() = default;

TriggerRegistration::~TriggerRegistration() = default;

TriggerRegistration::TriggerRegistration(const TriggerRegistration&) = default;

TriggerRegistration& TriggerRegistration::operator=(
    const TriggerRegistration&) = default;

TriggerRegistration::TriggerRegistration(TriggerRegistration&&) = default;

TriggerRegistration& TriggerRegistration::operator=(TriggerRegistration&&) =
    default;

// static
absl::optional<TriggerRegistration> TriggerRegistration::Create(
    url::Origin reporting_origin,
    Filters filters,
    Filters not_filters,
    absl::optional<uint64_t> debug_key,
    absl::optional<uint64_t> aggregatable_dedup_key,
    std::vector<EventTriggerData> event_triggers,
    std::vector<AggregatableTriggerData> aggregatable_trigger_data,
    AggregatableValues aggregatable_values,
    bool debug_reporting) {
  if (!SuitableOrigin::IsSuitable(reporting_origin))
    return absl::nullopt;

  TriggerRegistration result;
  result.reporting_origin_ = std::move(reporting_origin);
  result.filters_ = std::move(filters);
  result.not_filters_ = std::move(not_filters);
  result.debug_key_ = debug_key;
  result.aggregatable_dedup_key_ = aggregatable_dedup_key;
  result.event_triggers_ = std::move(event_triggers);
  result.aggregatable_trigger_data_ = std::move(aggregatable_trigger_data);
  result.aggregatable_values_ = std::move(aggregatable_values);
  result.debug_reporting_ = debug_reporting;
  return result;
}

}  // namespace attribution_reporting
