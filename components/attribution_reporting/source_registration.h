// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SourceRegistration {
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
  Parse(base::Value::Dict, SuitableOrigin reporting_origin);

  SourceRegistration(SuitableOrigin destination,
                     SuitableOrigin reporting_origin);

  ~SourceRegistration();

  SourceRegistration(const SourceRegistration&);
  SourceRegistration& operator=(const SourceRegistration&);

  SourceRegistration(SourceRegistration&&);
  SourceRegistration& operator=(SourceRegistration&&);

  uint64_t source_event_id = 0;
  SuitableOrigin destination;
  SuitableOrigin reporting_origin;
  absl::optional<base::TimeDelta> expiry;
  absl::optional<base::TimeDelta> event_report_window;
  absl::optional<base::TimeDelta> aggregatable_report_window;
  int64_t priority = 0;
  FilterData filter_data;
  absl::optional<uint64_t> debug_key;
  AggregationKeys aggregation_keys;
  bool debug_reporting = false;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
