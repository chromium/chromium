// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerRegistration {
  // Doesn't log metric on parsing failures.
  static base::expected<TriggerRegistration, mojom::TriggerRegistrationError>
      Parse(base::Value::Dict);

  // Logs metric on parsing failures.
  static base::expected<TriggerRegistration, mojom::TriggerRegistrationError>
  Parse(base::StringPiece json);

  TriggerRegistration();

  TriggerRegistration(FilterPair,
                      absl::optional<uint64_t> debug_key,
                      AggregatableDedupKeyList aggregatable_dedup_keys,
                      EventTriggerDataList event_triggers,
                      AggregatableTriggerDataList aggregatable_trigger_data,
                      AggregatableValues aggregatable_values,
                      bool debug_reporting,
                      aggregation_service::mojom::AggregationCoordinator
                          aggregation_coordinator);

  ~TriggerRegistration();

  TriggerRegistration(const TriggerRegistration&);
  TriggerRegistration& operator=(const TriggerRegistration&);

  TriggerRegistration(TriggerRegistration&&);
  TriggerRegistration& operator=(TriggerRegistration&&);

  base::Value::Dict ToJson() const;

  FilterPair filters;
  absl::optional<uint64_t> debug_key;
  AggregatableDedupKeyList aggregatable_dedup_keys;
  EventTriggerDataList event_triggers;
  AggregatableTriggerDataList aggregatable_trigger_data;
  AggregatableValues aggregatable_values;
  bool debug_reporting = false;
  aggregation_service::mojom::AggregationCoordinator aggregation_coordinator =
      aggregation_service::mojom::AggregationCoordinator::kDefault;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
