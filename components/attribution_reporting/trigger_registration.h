// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class AggregatableTriggerData;

struct AggregatableDedupKey;
struct EventTriggerData;

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerRegistration {
  // Doesn't log metric on parsing failures.
  static base::expected<TriggerRegistration, mojom::TriggerRegistrationError>
      Parse(base::Value::Dict);

  // Logs metric on parsing failures.
  static base::expected<TriggerRegistration, mojom::TriggerRegistrationError>
  Parse(base::StringPiece json);

  TriggerRegistration();

  TriggerRegistration(
      FilterPair,
      absl::optional<uint64_t> debug_key,
      std::vector<AggregatableDedupKey> aggregatable_dedup_keys,
      std::vector<EventTriggerData> event_triggers,
      std::vector<AggregatableTriggerData> aggregatable_trigger_data,
      AggregatableValues aggregatable_values,
      bool debug_reporting,
      absl::optional<SuitableOrigin> aggregation_coordinator_origin,
      mojom::SourceRegistrationTimeConfig source_registration_time_config);

  ~TriggerRegistration();

  TriggerRegistration(const TriggerRegistration&);
  TriggerRegistration& operator=(const TriggerRegistration&);

  TriggerRegistration(TriggerRegistration&&);
  TriggerRegistration& operator=(TriggerRegistration&&);

  base::Value::Dict ToJson() const;

  FilterPair filters;
  absl::optional<uint64_t> debug_key;
  std::vector<AggregatableDedupKey> aggregatable_dedup_keys;
  std::vector<EventTriggerData> event_triggers;
  std::vector<AggregatableTriggerData> aggregatable_trigger_data;
  AggregatableValues aggregatable_values;
  bool debug_reporting = false;
  absl::optional<SuitableOrigin> aggregation_coordinator_origin;
  attribution_reporting::mojom::SourceRegistrationTimeConfig
      source_registration_time_config =
          attribution_reporting::mojom::SourceRegistrationTimeConfig::kExclude;

  // When adding new members, the corresponding `operator==()` definition in
  // `test_utils.h` should also be updated.
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
