// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_win.h"

#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {
DomainEnrollmentStatusProvider::DomainEnrollmentStatusProvider() = default;

DomainEnrollmentStatusProvider::~DomainEnrollmentStatusProvider() = default;

EnterpriseManagementAuthority DomainEnrollmentStatusProvider::GetAuthority() {
  return DomainEnrollmentStatusProvider::IsEnrolledToDomain()
             ? EnterpriseManagementAuthority::DOMAIN_LOCAL
             : EnterpriseManagementAuthority::NONE;
}

bool DomainEnrollmentStatusProvider::IsEnrolledToDomain() {
  return base::win::IsEnrolledToDomain();
}

EnterpriseMDMManagementStatusProvider::EnterpriseMDMManagementStatusProvider() =
    default;

EnterpriseMDMManagementStatusProvider::
    ~EnterpriseMDMManagementStatusProvider() = default;

EnterpriseManagementAuthority
EnterpriseMDMManagementStatusProvider::GetAuthority() {
  return base::win::OSInfo::GetInstance()->version_type() !=
                     base::win::SUITE_HOME &&
                 base::win::IsDeviceRegisteredWithManagement()
             ? EnterpriseManagementAuthority::CLOUD
             : EnterpriseManagementAuthority::NONE;
}

}  // namespace policy
