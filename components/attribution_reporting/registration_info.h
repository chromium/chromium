// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_INFO_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_INFO_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "net/http/structured_headers.h"

namespace attribution_reporting {

enum class Registrar;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RegistrationInfoError)
enum class RegistrationInfoError {
  kRootInvalid = 0,
  kInvalidPreferredPlatform = 1,
  kInvalidReportHeaderErrors = 2,
  kMaxValue = kInvalidReportHeaderErrors,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionRegistrationInfoError)

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) RegistrationInfo {
  std::optional<Registrar> preferred_platform;
  bool report_header_errors = false;

  // Parses an Attribution-Reporting-Info header for the registration info.
  //
  // Currently only 'preferred-platform' and 'report-header-errors' fields are
  // expected in the Attribution-Reporting-Info header. Any other fields are
  // ignored. Returns an error if the string is not parsable as a
  // structured-header dictionary or the value of 'preferred-platform' is not an
  // allowed token or the value of 'report-header-errors' is not a boolean.
  //
  // Example:
  //
  // `preferred-platform=web` or `preferred-platform=os` or
  // `report-header-errors` or `preferred-platform=web,report-header-errors`
  static base::expected<RegistrationInfo, RegistrationInfoError> ParseInfo(
      std::string_view,
      bool cross_app_web_enabled);

  // Same as the above, but using an already-parsed structured-header
  // dictionary.
  static base::expected<RegistrationInfo, RegistrationInfoError> ParseInfo(
      const net::structured_headers::Dictionary&,
      bool cross_app_web_enabled);

  friend bool operator==(const RegistrationInfo&,
                         const RegistrationInfo&) = default;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void RecordRegistrationInfoError(RegistrationInfoError);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_INFO_H_
