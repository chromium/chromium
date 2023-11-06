// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_MAX_EVENT_LEVEL_REPORTS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_MAX_EVENT_LEVEL_REPORTS_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"

namespace attribution_reporting {

// Represents the maximum number of event-level reports that can be created for
// a single attribution source.
//
// https://wicg.github.io/attribution-reporting-api/#attribution-source-max-number-of-event-level-reports
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) MaxEventLevelReports {
 public:
  static base::expected<MaxEventLevelReports, mojom::SourceRegistrationError>
  Parse(const base::Value::Dict&, mojom::SourceType);

  // https://wicg.github.io/attribution-reporting-api/#max-settable-event-level-attributions-per-source
  static MaxEventLevelReports Max();

  MaxEventLevelReports() = default;

  // `CHECK()`s that the given value is valid.
  explicit MaxEventLevelReports(int);

  // https://wicg.github.io/attribution-reporting-api/#default-event-level-attributions-per-source
  explicit MaxEventLevelReports(mojom::SourceType);

  MaxEventLevelReports(const MaxEventLevelReports&) = default;
  MaxEventLevelReports& operator=(const MaxEventLevelReports&) = default;

  MaxEventLevelReports(MaxEventLevelReports&&) = default;
  MaxEventLevelReports& operator=(MaxEventLevelReports&&) = default;

  // This implicit conversion is allowed to ease drop-in use of
  // this type in places currently requiring `int` with prior validation.
  operator int() const {  // NOLINT
    return max_event_level_reports_;
  }

  [[nodiscard]] bool SetIfValid(int);

  void Serialize(base::Value::Dict&) const;

 private:
  int max_event_level_reports_ = 0;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_MAX_EVENT_LEVEL_REPORTS_H_
