// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/types/strong_alias.h"
#include "components/attribution_reporting/os_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace attribution_reporting {

using OsSourceRegistrationError =
    base::StrongAlias<struct OsSourceRegistrationErrorTag,
                      mojom::OsRegistrationError>;

using OsTriggerRegistrationError =
    base::StrongAlias<struct OsTriggerRegistrationErrorTag,
                      mojom::OsRegistrationError>;

using RegistrationHeaderErrorDetails =
    absl::variant<mojom::SourceRegistrationError,
                  mojom::TriggerRegistrationError,
                  OsSourceRegistrationError,
                  OsTriggerRegistrationError>;

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) RegistrationHeaderError {
  std::string header_value;
  RegistrationHeaderErrorDetails error_details;

  RegistrationHeaderError() = default;

  RegistrationHeaderError(std::string_view header_value,
                          RegistrationHeaderErrorDetails error_details)
      : header_value(header_value), error_details(error_details) {}

  std::string_view HeaderName() const;

  friend bool operator==(const RegistrationHeaderError&,
                         const RegistrationHeaderError&) = default;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_
