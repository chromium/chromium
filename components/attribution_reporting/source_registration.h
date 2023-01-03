// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
struct DefaultConstructTraits;
}  // namespace mojo

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void RecordSourceRegistrationError(mojom::SourceRegistrationError);

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SourceRegistration {
  // Doesn't log metric on parsing failures.
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
      Parse(base::Value::Dict);

  // Logs metric on parsing failures.
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
  Parse(base::StringPiece json);

  explicit SourceRegistration(SuitableOrigin destination);

  ~SourceRegistration();

  SourceRegistration(const SourceRegistration&);
  SourceRegistration& operator=(const SourceRegistration&);

  SourceRegistration(SourceRegistration&&);
  SourceRegistration& operator=(SourceRegistration&&);

  base::Value::Dict ToJson() const;

  uint64_t source_event_id = 0;
  SuitableOrigin destination;
  absl::optional<base::TimeDelta> expiry;
  absl::optional<base::TimeDelta> event_report_window;
  absl::optional<base::TimeDelta> aggregatable_report_window;
  int64_t priority = 0;
  FilterData filter_data;
  absl::optional<uint64_t> debug_key;
  AggregationKeys aggregation_keys;
  bool debug_reporting = false;

 private:
  friend mojo::DefaultConstructTraits;

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  SourceRegistration();
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
