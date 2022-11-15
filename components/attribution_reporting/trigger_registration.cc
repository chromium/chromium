// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <utility>

#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/suitable_origin.h"

namespace attribution_reporting {

TriggerRegistration::TriggerRegistration(
    SuitableOrigin reporting_origin,
    Filters filters,
    Filters not_filters,
    absl::optional<uint64_t> debug_key,
    absl::optional<uint64_t> aggregatable_dedup_key,
    EventTriggerDataList event_triggers,
    AggregatableTriggerDataList aggregatable_trigger_data,
    AggregatableValues aggregatable_values,
    bool debug_reporting)
    : reporting_origin(std::move(reporting_origin)),
      filters(std::move(filters)),
      not_filters(std::move(not_filters)),
      debug_key(debug_key),
      aggregatable_dedup_key(aggregatable_dedup_key),
      event_triggers(std::move(event_triggers)),
      aggregatable_trigger_data(aggregatable_trigger_data),
      aggregatable_values(std::move(aggregatable_values)),
      debug_reporting(debug_reporting) {}

TriggerRegistration::~TriggerRegistration() = default;

TriggerRegistration::TriggerRegistration(const TriggerRegistration&) = default;

TriggerRegistration& TriggerRegistration::operator=(
    const TriggerRegistration&) = default;

TriggerRegistration::TriggerRegistration(TriggerRegistration&&) = default;

TriggerRegistration& TriggerRegistration::operator=(TriggerRegistration&&) =
    default;

}  // namespace attribution_reporting
