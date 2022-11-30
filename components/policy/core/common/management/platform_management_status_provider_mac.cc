// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_mac.h"

#include "base/enterprise_util.h"
#include "components/policy/core/common/policy_pref_names.h"

namespace policy {

DomainEnrollmentStatusProvider::DomainEnrollmentStatusProvider() {
  domain_join_state_ = base::AreDeviceAndUserJoinedToDomain();
}

DomainEnrollmentStatusProvider::~DomainEnrollmentStatusProvider() = default;

EnterpriseManagementAuthority DomainEnrollmentStatusProvider::FetchAuthority() {
  return base::IsEnterpriseDevice()
             ? EnterpriseManagementAuthority::DOMAIN_LOCAL
             : EnterpriseManagementAuthority::NONE;
}

EnterpriseMDMManagementStatusProvider::EnterpriseMDMManagementStatusProvider()
    : ManagementStatusProvider(policy_prefs::kEnterpriseMDMManagementMac) {}

EnterpriseMDMManagementStatusProvider::
    ~EnterpriseMDMManagementStatusProvider() = default;

EnterpriseManagementAuthority
EnterpriseMDMManagementStatusProvider::FetchAuthority() {
  return base::IsManagedDevice() ? EnterpriseManagementAuthority::CLOUD
                                 : EnterpriseManagementAuthority::NONE;
}

}  // namespace policy
