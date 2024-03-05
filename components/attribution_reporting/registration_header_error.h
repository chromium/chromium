// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "components/attribution_reporting/registration_header_type.mojom-forward.h"

namespace attribution_reporting {

struct RegistrationHeaderError {
  mojom::RegistrationHeaderType header_type;
  std::string header_value;

  // TODO(linnan): Consider including error details.

  RegistrationHeaderError() = default;

  RegistrationHeaderError(mojom::RegistrationHeaderType header_type,
                          std::string_view header_value)
      : header_type(header_type), header_value(header_value) {}
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_H_
