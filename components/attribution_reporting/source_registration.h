// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void RecordSourceRegistrationError(mojom::SourceRegistrationError);

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SourceRegistration {
  // Doesn't log metric on parsing failures.
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
      Parse(base::Value, mojom::SourceType);

  // Logs metric on parsing failures.
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
  Parse(std::string_view json, mojom::SourceType);

  explicit SourceRegistration(DestinationSet);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit SourceRegistration(mojo::DefaultConstruct::Tag);

  ~SourceRegistration();

  SourceRegistration(const SourceRegistration&);
  SourceRegistration& operator=(const SourceRegistration&);

  SourceRegistration(SourceRegistration&&);
  SourceRegistration& operator=(SourceRegistration&&);

  base::Value::Dict ToJson() const;

  bool IsValid() const;
  bool IsValidForSourceType(mojom::SourceType) const;

  friend bool operator==(const SourceRegistration&,
                         const SourceRegistration&) = default;

  uint64_t source_event_id = 0;
  DestinationSet destination_set;
  // These `base::TimeDelta`s must be non-negative if set. This is verified by
  // the `Parse()` and `IsValid()` methods.
  base::TimeDelta expiry = kMaxSourceExpiry;
  TriggerSpecs trigger_specs;
  base::TimeDelta aggregatable_report_window = expiry;
  int64_t priority = 0;
  FilterData filter_data;
  std::optional<uint64_t> debug_key;
  AggregationKeys aggregation_keys;
  bool debug_reporting = false;
  mojom::TriggerDataMatching trigger_data_matching =
      mojom::TriggerDataMatching::kModulus;
  EventLevelEpsilon event_level_epsilon;
  SourceAggregatableDebugReportingConfig aggregatable_debug_reporting_config;
  int64_t destination_limit_priority = 0;
  std::optional<AttributionScopesData> attribution_scopes_data;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
