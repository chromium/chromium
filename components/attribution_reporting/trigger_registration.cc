// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <utility>

#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

base::expected<EventTriggerDataList, TriggerRegistrationError>
ParseEventTriggerDataList(base::Value* value) {
  if (!value)
    return EventTriggerDataList();

  base::Value::List* list = value->GetIfList();
  if (!list) {
    return base::unexpected(
        TriggerRegistrationError::kEventTriggerDataListWrongType);
  }

  return EventTriggerDataList::Build(
      *list, TriggerRegistrationError::kEventTriggerDataListTooLong,
      &EventTriggerData::FromJSON);
}

base::expected<AggregatableTriggerDataList, TriggerRegistrationError>
ParseAggregatableTriggerDataList(base::Value* value) {
  if (!value)
    return AggregatableTriggerDataList();

  base::Value::List* list = value->GetIfList();
  if (!list) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataListWrongType);
  }

  return AggregatableTriggerDataList::Build(
      *list, TriggerRegistrationError::kAggregatableTriggerDataListTooLong,
      &AggregatableTriggerData::FromJSON);
}

}  // namespace

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value::Dict registration,
                           SuitableOrigin reporting_origin) {
  auto filters = Filters::FromJSON(registration.Find("filters"));
  if (!filters.has_value())
    return base::unexpected(filters.error());

  auto not_filters = Filters::FromJSON(registration.Find("not_filters"));
  if (!not_filters.has_value())
    return base::unexpected(not_filters.error());

  auto event_triggers =
      ParseEventTriggerDataList(registration.Find("event_trigger_data"));
  if (!event_triggers.has_value())
    return base::unexpected(event_triggers.error());

  auto aggregatable_trigger_data = ParseAggregatableTriggerDataList(
      registration.Find("aggregatable_trigger_data"));
  if (!aggregatable_trigger_data.has_value())
    return base::unexpected(aggregatable_trigger_data.error());

  auto aggregatable_values =
      AggregatableValues::FromJSON(registration.Find("aggregatable_values"));
  if (!aggregatable_values.has_value())
    return base::unexpected(aggregatable_values.error());

  absl::optional<uint64_t> debug_key = ParseDebugKey(registration);
  absl::optional<uint64_t> aggregatable_dedup_key =
      ParseUint64(registration, "aggregatable_deduplication_key");
  bool debug_reporting = ParseDebugReporting(registration);

  return TriggerRegistration(std::move(reporting_origin), std::move(*filters),
                             std::move(*not_filters), debug_key,
                             aggregatable_dedup_key, std::move(*event_triggers),
                             std::move(*aggregatable_trigger_data),
                             std::move(*aggregatable_values), debug_reporting);
}

TriggerRegistration::TriggerRegistration(SuitableOrigin reporting_origin)
    : reporting_origin(std::move(reporting_origin)) {}

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
