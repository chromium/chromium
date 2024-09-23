// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/eligibility.h"

#include <optional>

#include "components/attribution_reporting/registration_eligibility.mojom-shared.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace attribution_reporting {

namespace {
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::network::mojom::AttributionReportingEligibility;
}  // namespace

std::optional<RegistrationEligibility> GetRegistrationEligibility(
    AttributionReportingEligibility net_value) {
  switch (net_value) {
    case AttributionReportingEligibility::kEmpty:
      return std::nullopt;
    case AttributionReportingEligibility::kUnset:
    case AttributionReportingEligibility::kTrigger:
      return RegistrationEligibility::kTrigger;
    case AttributionReportingEligibility::kEventSource:
    case AttributionReportingEligibility::kNavigationSource:
      return RegistrationEligibility::kSource;
    case AttributionReportingEligibility::kEventSourceOrTrigger:
      return RegistrationEligibility::kSourceOrTrigger;
  }
  return std::nullopt;
}

}  // namespace attribution_reporting
