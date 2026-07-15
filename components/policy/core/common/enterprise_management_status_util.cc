// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_status_util.h"

#include <string>

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

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

}  // namespace policy
