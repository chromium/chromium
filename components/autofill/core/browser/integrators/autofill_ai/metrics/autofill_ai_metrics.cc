// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

void LogOptInFunnelEvent(AutofillAiOptInFunnelEvents event) {
  base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Funnel", event);
  // TODO(crbug.com/408380915): Remove after M141.
  base::UmaHistogramEnumeration("Autofill.Ai.OptInFunnel", event);
}

// LINT.IfChange(EntityTypeToMetricsString)
std::string_view EntityTypeToMetricsString(EntityType type) {
  switch (type.name()) {
    case EntityTypeName::kPassport:
      return "Passport";
    case EntityTypeName::kDriversLicense:
      return "DriversLicense";
    case EntityTypeName::kVehicle:
      return "Vehicle";
    case EntityTypeName::kNationalIdCard:
      return "NationalIdCard";
    case EntityTypeName::kKnownTravelerNumber:
      return "KnownTravelerNumber";
    case EntityTypeName::kRedressNumber:
      return "RedressNumber";
    case EntityTypeName::kFlightReservation:
      return "FlightReservation";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiEntityType)

}  // namespace autofill
