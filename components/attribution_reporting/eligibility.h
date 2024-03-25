// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ELIGIBILITY_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ELIGIBILITY_H_

#include <optional>

#include "base/component_export.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::optional<mojom::RegistrationEligibility> GetRegistrationEligibility(
    network::mojom::AttributionReportingEligibility);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ELIGIBILITY_H_
