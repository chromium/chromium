// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include "base/containers/flat_map.h"

namespace policy {

namespace {

base::flat_map<ManagementTarget, base::flat_set<EnterpriseManagementAuthority>>*
    g_management_authorities_overrides = nullptr;

ManagementAuthorityTrustworthiness GetTrustworthiness(
    const base::flat_set<EnterpriseManagementAuthority> authorities) {
  if (authorities.find(EnterpriseManagementAuthority::CLOUD_DOMAIN) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::FULLY_TRUSTED;
  if (authorities.find(EnterpriseManagementAuthority::CLOUD) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (authorities.find(EnterpriseManagementAuthority::DOMAIN_LOCAL) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (authorities.find(EnterpriseManagementAuthority::COMPUTER_LOCAL) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::LOW;
  return ManagementAuthorityTrustworthiness::NONE;
}

}  // namespace

ManagementStatusProvider::~ManagementStatusProvider() = default;

ManagementService::ManagementService(ManagementTarget target)
    : target_(target) {}
ManagementService::~ManagementService() = default;

base::flat_set<EnterpriseManagementAuthority>
ManagementService::GetManagementAuthorities() {
  if (g_management_authorities_overrides) {
    auto values_for_testing = g_management_authorities_overrides->find(target_);
    if (values_for_testing != g_management_authorities_overrides->end())
      return values_for_testing->second;
  }
  base::flat_set<EnterpriseManagementAuthority> result;
  for (const auto& delegate : management_status_providers_) {
    if (delegate->IsManaged())
      result.insert(delegate->GetAuthority());
  }
  return result;
}

ManagementAuthorityTrustworthiness
ManagementService::GetManagementAuthorityTrustworthiness() {
  return GetTrustworthiness(GetManagementAuthorities());
}

bool ManagementService::IsManaged() {
  return GetManagementAuthorityTrustworthiness() >
         ManagementAuthorityTrustworthiness::NONE;
}

void ManagementService::SetManagementStatusProvider(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
  management_status_providers_ = std::move(providers);
}

void ManagementService::SetManagementAuthoritiesForTesting(
    ManagementTarget target,
    base::flat_set<EnterpriseManagementAuthority> authorities) {
  if (!g_management_authorities_overrides)
    g_management_authorities_overrides =
        new base::flat_map<ManagementTarget,
                           base::flat_set<EnterpriseManagementAuthority>>();
  DCHECK(g_management_authorities_overrides->find(target) ==
         g_management_authorities_overrides->end());
  g_management_authorities_overrides->insert(
      std::make_pair(target, std::move(authorities)));
}

void ManagementService::RemoveManagementAuthoritiesForTesting(
    ManagementTarget target) {
  DCHECK(g_management_authorities_overrides);
  g_management_authorities_overrides->erase(target);
  if (g_management_authorities_overrides->empty()) {
    delete g_management_authorities_overrides;
    g_management_authorities_overrides = nullptr;
  }
}

}  // namespace policy
