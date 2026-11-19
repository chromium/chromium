// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_STATUS_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_STATUS_UTIL_H_

#include <string_view>
#include <vector>

#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

inline constexpr std::string_view kManagementServiceKey = "managementService";
inline constexpr std::string_view kPlatformManagementStatusKey =
    "platformManagementStatus";
inline constexpr std::string_view kBrowserManagementStatusKey =
    "browserManagementStatus";

class ManagementService;
class PolicyService;

// LINT.IfChange(PlatformManagementStatus)
enum class PlatformManagementStatus {
  kUnmanaged = 0,
  kComputerLocal = 1,
  kDomainLocal = 2,
  kCloud = 3,
  kCloudDomain = 4,
  kMaxValue = kCloudDomain,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:PlatformManagementStatus)

// LINT.IfChange(BrowserManagementStatus)
enum class BrowserManagementStatus {
  kUnmanaged = 0,
  kComputerLocalLe3 = 1,
  kComputerLocalGt3 = 2,
  kDomainLocal = 3,
  kCloud = 4,
  kCloudDomain = 5,
  kMaxValue = kCloudDomain,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:BrowserManagementStatus)

// Extracts and maps the platform management status(es) from the platform management service.
std::vector<PlatformManagementStatus> GetPlatformManagementStatuses(
    ManagementService* platform_management_service);

// Extracts and maps the browser management status(es) from the browser management service and policies.
std::vector<BrowserManagementStatus> GetBrowserManagementStatuses(
    ManagementService* browser_management_service,
    PolicyService* policy_service);

// Returns human-readable platform and browser management statuses for
// diagnostic output.
POLICY_EXPORT base::DictValue GetManagementServiceStatus(
    ManagementService* platform_management_service,
    ManagementService* browser_management_service,
    PolicyService* policy_service);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_STATUS_UTIL_H_
