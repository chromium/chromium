// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_trigger_data.h"

#include <stdint.h>

#include <utility>

#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kDeduplicationKey[] = "deduplication_key";
constexpr char kTriggerData[] = "trigger_data";

}  // namespace

// static
base::expected<EventTriggerData, TriggerRegistrationError>
EventTriggerData::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kEventTriggerDataWrongType);
  }

  auto filters = Filters::FromJSON(dict->Find(Filters::kFilters));
  if (!filters.has_value())
    return base::unexpected(filters.error());

  auto not_filters = Filters::FromJSON(dict->Find(Filters::kNotFilters));
  if (!not_filters.has_value())
    return base::unexpected(not_filters.error());

  uint64_t data = ParseUint64(*dict, kTriggerData).value_or(0);
  int64_t priority = ParsePriority(*dict);
  absl::optional<uint64_t> dedup_key = ParseUint64(*dict, kDeduplicationKey);

  return EventTriggerData(data, priority, dedup_key, std::move(*filters),
                          std::move(*not_filters));
}

EventTriggerData::EventTriggerData() = default;

EventTriggerData::EventTriggerData(uint64_t data,
                                   int64_t priority,
                                   absl::optional<uint64_t> dedup_key,
                                   Filters filters,
                                   Filters not_filters)
    : data(data),
      priority(priority),
      dedup_key(dedup_key),
      filters(std::move(filters)),
      not_filters(std::move(not_filters)) {}

base::Value::Dict EventTriggerData::ToJson() const {
  base::Value::Dict dict;

  filters.SerializeIfNotEmpty(dict, Filters::kFilters);
  not_filters.SerializeIfNotEmpty(dict, Filters::kNotFilters);

  SerializeUint64(dict, kTriggerData, data);
  SerializePriority(dict, priority);

  if (dedup_key) {
    SerializeUint64(dict, kDeduplicationKey, *dedup_key);
  }

  return dict;
}

}  // namespace attribution_reporting
