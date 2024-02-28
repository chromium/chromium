// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "net/http/structured_headers.h"

namespace attribution_reporting {

enum class Registrar {
  kWeb,
  kOs,
};

struct PreferredPlatformError {
  friend bool operator==(const PreferredPlatformError&,
                         const PreferredPlatformError&) = default;
};

// Parses an Attribution-Reporting-Info header for the preferred platform.
//
// Currently only 'preferred-platform' field is expected in the
// Attribution-Reporting-Info header. Any other fields are ignored.
// Returns an error if the string is not parsable as a structured-header
// dictionary or the value of 'preferred-platform' is not an allowed token.
//
// Example:
//
// `preferred-platform=web` or `preferred-platform=os`
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::optional<Registrar>, PreferredPlatformError> ParseInfo(
    std::string_view);

// Same as the above, but using an already-parsed structured-header dictionary.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::optional<Registrar>, PreferredPlatformError> ParseInfo(
    const net::structured_headers::Dictionary&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_H_
