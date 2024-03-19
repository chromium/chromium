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
#include "components/attribution_reporting/registration_header_type.mojom-forward.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace base {
class Value;
}  // namespace base

namespace attribution_reporting {

using OsSourceRegistrationError =
    base::StrongAlias<struct OsSourceRegistrationErrorTag,
                      mojom::OsRegistrationError>;

using OsTriggerRegistrationError =
    base::StrongAlias<struct OsTriggerRegistrationErrorTag,
                      mojom::OsRegistrationError>;

struct RegistrationHeaderError {
  mojom::RegistrationHeaderType header_type;
  std::string header_value;

  // TODO(linnan): Consider including error details.

  RegistrationHeaderError() = default;

  RegistrationHeaderError(mojom::RegistrationHeaderType header_type,
                          std::string_view header_value)
      : header_type(header_type), header_value(header_value) {}
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Value ErrorDetails(mojom::SourceRegistrationError);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Value ErrorDetails(mojom::TriggerRegistrationError);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Value ErrorDetails(OsSourceRegistrationError);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Value ErrorDetails(OsTriggerRegistrationError);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_
