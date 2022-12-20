// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/eligibility.h"

#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/eligibility_error.mojom-shared.h"
#include "components/attribution_reporting/registration_type.mojom-shared.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::EligibilityError;
using ::attribution_reporting::mojom::RegistrationType;

}  // namespace

base::expected<mojom::RegistrationType, mojom::EligibilityError>
ParseEligibleHeader(absl::optional<base::StringPiece> header) {
  // All subresources are eligible to register triggers if they do *not*
  // specify the header.
  if (!header.has_value()) {
    return RegistrationType::kTrigger;
  }

  absl::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(*header);
  if (!dict) {
    return base::unexpected(EligibilityError::kInvalidStructuredHeader);
  }

  if (dict->contains("navigation-source")) {
    return base::unexpected(EligibilityError::kContainsNavigationSource);
  }

  const bool allows_event_source = dict->contains("event-source");
  const bool allows_trigger = dict->contains("trigger");

  if (allows_event_source && allows_trigger) {
    return RegistrationType::kSourceOrTrigger;
  }

  if (allows_event_source) {
    return RegistrationType::kSource;
  }

  if (allows_trigger) {
    return RegistrationType::kTrigger;
  }

  return base::unexpected(EligibilityError::kIneligible);
}

}  // namespace attribution_reporting
