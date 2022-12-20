// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ELIGIBILITY_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ELIGIBILITY_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/eligibility_error.mojom-forward.h"
#include "components/attribution_reporting/registration_type.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

// Parses an Attribution-Reporting-Eligible header as a structured-header
// dictionary.
//
// The structured-header items may have values and/or parameters, but they are
// ignored.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<mojom::RegistrationType, mojom::EligibilityError>
    ParseEligibleHeader(absl::optional<base::StringPiece>);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ELIGIBILITY_H_
