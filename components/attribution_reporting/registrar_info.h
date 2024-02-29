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

enum class IssueType {
  kWebAndOsHeaders,
  kSourceIgnored,
  kTriggerIgnored,
  kOsSourceIgnored,
  kOsTriggerIgnored,
  kNoRegisterSourceHeader,
  kNoRegisterTriggerHeader,
  kNoRegisterOsSourceHeader,
  kNoRegisterOsTriggerHeader,

  kMinValue = kWebAndOsHeaders,
  kMaxValue = kNoRegisterOsTriggerHeader,
};

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

  std::optional<Registrar> registrar;
  IssueTypes issues;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRAR_INFO_H_
