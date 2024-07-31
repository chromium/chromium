// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_INFO_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_INFO_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/types/expected.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"

namespace attribution_reporting {

enum class Registrar;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IssueType)
enum class IssueType {
  kWebAndOsHeaders = 0,
  kWebIgnored = 1,
  kOsIgnored = 2,
  kNoWebHeader = 3,
  kNoOsHeader = 4,

  kMinValue = kWebAndOsHeaders,
  kMaxValue = kNoOsHeader,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionRegistrationRegistrarIssue)

using IssueTypes =
    base::EnumSet<IssueType, IssueType::kMinValue, IssueType::kMaxValue>;

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) RegistrarInfo {
  static RegistrarInfo Get(bool has_web_header,
                           bool has_os_header,
                           bool is_source,
                           std::optional<Registrar> preferred_platform,
                           network::mojom::AttributionSupport);

  RegistrarInfo();
  ~RegistrarInfo();

  RegistrarInfo(const RegistrarInfo&);
  RegistrarInfo& operator=(const RegistrarInfo&);

  RegistrarInfo(RegistrarInfo&&);
  RegistrarInfo& operator=(RegistrarInfo&&);

  friend bool operator==(const RegistrarInfo&, const RegistrarInfo&) = default;

  std::optional<Registrar> registrar;
  IssueTypes issues;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_INFO_H_
