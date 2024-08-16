// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_

#include <stdint.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class AggregatableTriggerData;
class AggregatableValues;

struct AggregatableDedupKey;
struct EventTriggerData;

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void RecordTriggerRegistrationError(mojom::TriggerRegistrationError);

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerRegistration {
  // Doesn't log metric on parsing failures.
  static base::expected<TriggerRegistration, mojom::TriggerRegistrationError>
      Parse(base::Value);

  // Logs metric on parsing failures.
  static base::expected<TriggerRegistration, mojom::TriggerRegistrationError>
  Parse(std::string_view json);

  TriggerRegistration();

  ~TriggerRegistration();

  TriggerRegistration(const TriggerRegistration&);
  TriggerRegistration& operator=(const TriggerRegistration&);

  TriggerRegistration(TriggerRegistration&&);
  TriggerRegistration& operator=(TriggerRegistration&&);

  base::Value::Dict ToJson() const;

  bool IsValid() const;

  friend bool operator==(const TriggerRegistration&,
                         const TriggerRegistration&) = default;

  FilterPair filters;
  std::optional<uint64_t> debug_key;
  std::vector<AggregatableDedupKey> aggregatable_dedup_keys;
  std::vector<EventTriggerData> event_triggers;
  std::vector<AggregatableTriggerData> aggregatable_trigger_data;
  std::vector<AggregatableValues> aggregatable_values;
  bool debug_reporting = false;
  std::optional<SuitableOrigin> aggregation_coordinator_origin;
  AggregatableTriggerConfig aggregatable_trigger_config;
  AggregatableDebugReportingConfig aggregatable_debug_reporting_config;
  AttributionScopesSet attribution_scopes;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
