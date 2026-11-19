// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_status_util.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

namespace {

std::string_view GetPlatformManagementStatusName(
    PlatformManagementStatus status) {
  switch (status) {
    case PlatformManagementStatus::kUnmanaged:
      return "Unmanaged";
    case PlatformManagementStatus::kComputerLocal:
      return "Computer Local";
    case PlatformManagementStatus::kDomainLocal:
      return "Domain Local";
    case PlatformManagementStatus::kCloud:
      return "Cloud";
    case PlatformManagementStatus::kCloudDomain:
      return "Cloud Domain";
  }
  NOTREACHED();
}

std::string_view GetBrowserManagementStatusName(
    BrowserManagementStatus status) {
  switch (status) {
    case BrowserManagementStatus::kUnmanaged:
      return "Unmanaged";
    case BrowserManagementStatus::kComputerLocalLe3:
      return "Computer Local (<= 3 policies)";
    case BrowserManagementStatus::kComputerLocalGt3:
      return "Computer Local (> 3 policies)";
    case BrowserManagementStatus::kDomainLocal:
      return "Domain Local";
    case BrowserManagementStatus::kCloud:
      return "Cloud";
    case BrowserManagementStatus::kCloudDomain:
      return "Cloud Domain";
  }
  NOTREACHED();
}

}  // namespace

std::vector<PlatformManagementStatus> GetPlatformManagementStatuses(
    ManagementService* platform_management_service) {
  std::vector<PlatformManagementStatus> statuses;
  if (!platform_management_service || !platform_management_service->IsManaged()) {
    statuses.push_back(PlatformManagementStatus::kUnmanaged);
    return statuses;
  }

  if (platform_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    statuses.push_back(PlatformManagementStatus::kCloudDomain);
  }
  if (platform_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::CLOUD)) {
    statuses.push_back(PlatformManagementStatus::kCloud);
  }
  if (platform_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::DOMAIN_LOCAL)) {
    statuses.push_back(PlatformManagementStatus::kDomainLocal);
  }
  if (platform_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::COMPUTER_LOCAL)) {
    statuses.push_back(PlatformManagementStatus::kComputerLocal);
  }
  return statuses;
}

std::vector<BrowserManagementStatus> GetBrowserManagementStatuses(
    ManagementService* browser_management_service,
    PolicyService* policy_service) {
  std::vector<BrowserManagementStatus> statuses;
  if (!browser_management_service || !browser_management_service->IsManaged()) {
    statuses.push_back(BrowserManagementStatus::kUnmanaged);
    return statuses;
  }

  if (browser_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    statuses.push_back(BrowserManagementStatus::kCloudDomain);
  }
  if (browser_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::CLOUD)) {
    statuses.push_back(BrowserManagementStatus::kCloud);
  }
  if (browser_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::DOMAIN_LOCAL)) {
    statuses.push_back(BrowserManagementStatus::kDomainLocal);
  }
  if (browser_management_service->HasManagementAuthority(
          EnterpriseManagementAuthority::COMPUTER_LOCAL)) {
    int policy_count = 0;
    if (policy_service) {
      const auto& policies = policy_service->GetPolicies(
          PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
      policy_count = static_cast<int>(policies.size());
    }
    if (policy_count <= 3) {
      statuses.push_back(BrowserManagementStatus::kComputerLocalLe3);
    } else {
      statuses.push_back(BrowserManagementStatus::kComputerLocalGt3);
    }
  }
  return statuses;
}

base::DictValue GetManagementServiceStatus(
    ManagementService* platform_management_service,
    ManagementService* browser_management_service,
    PolicyService* policy_service) {
  base::ListValue platform_status;
  for (PlatformManagementStatus status :
       GetPlatformManagementStatuses(platform_management_service)) {
    platform_status.Append(GetPlatformManagementStatusName(status));
  }

  base::ListValue browser_status;
  for (BrowserManagementStatus status : GetBrowserManagementStatuses(
           browser_management_service, policy_service)) {
    browser_status.Append(GetBrowserManagementStatusName(status));
  }

  return base::DictValue()
      .Set(kPlatformManagementStatusKey, std::move(platform_status))
      .Set(kBrowserManagementStatusKey, std::move(browser_status));
}

}  // namespace policy
