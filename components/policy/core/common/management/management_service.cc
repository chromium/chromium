// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include "base/containers/flat_map.h"

namespace policy {

ManagementStatusProvider::~ManagementStatusProvider() = default;

ManagementService::ManagementService(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers)
    : management_status_providers_(std::move(providers)) {}

ManagementService::~ManagementService() = default;

bool ManagementService::HasManagementAuthority(
    EnterpriseManagementAuthority authority) {
  return GetManagementAuthorities() & authority;
}

ManagementAuthorityTrustworthiness
ManagementService::GetManagementAuthorityTrustworthiness() {
  if (HasManagementAuthority(EnterpriseManagementAuthority::CLOUD_DOMAIN))
    return ManagementAuthorityTrustworthiness::FULLY_TRUSTED;
  if (HasManagementAuthority(EnterpriseManagementAuthority::CLOUD))
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (HasManagementAuthority(EnterpriseManagementAuthority::DOMAIN_LOCAL))
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (HasManagementAuthority(EnterpriseManagementAuthority::COMPUTER_LOCAL))
    return ManagementAuthorityTrustworthiness::LOW;
  return ManagementAuthorityTrustworthiness::NONE;
}

uint64_t ManagementService::GetManagementAuthorities() {
  if (management_authorities_for_testing_)
    return management_authorities_for_testing_.value();

  uint64_t result = 0;
  for (const auto& provider : management_status_providers_)
    result |= provider->GetAuthority();
  return result;
}

void ManagementService::SetManagementAuthoritiesForTesting(
    uint64_t management_authorities) {
  management_authorities_for_testing_ = management_authorities;
}

void ManagementService::ClearManagementAuthoritiesForTesting() {
  management_authorities_for_testing_.reset();
}

bool ManagementService::IsManaged() {
  return GetManagementAuthorityTrustworthiness() >
         ManagementAuthorityTrustworthiness::NONE;
}

void ManagementService::SetManagementStatusProvider(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
  management_status_providers_ = std::move(providers);
}

}  // namespace policy
