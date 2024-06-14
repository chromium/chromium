// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_header_error.h"

#include "base/functional/overloaded.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace attribution_reporting {

std::string_view RegistrationHeaderError::HeaderName() const {
  return absl::visit(base::Overloaded{
                         [](mojom::SourceRegistrationError) {
                           return kAttributionReportingRegisterSourceHeader;
                         },

                         [](mojom::TriggerRegistrationError) {
                           return kAttributionReportingRegisterTriggerHeader;
                         },

                         [](OsSourceRegistrationError) {
                           return kAttributionReportingRegisterOsSourceHeader;
                         },

                         [](OsTriggerRegistrationError) {
                           return kAttributionReportingRegisterOsTriggerHeader;
                         },
                     },
                     error_details);
}

}  // namespace attribution_reporting
