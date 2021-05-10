// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_win.h"

#include "components/policy/core/common/cloud/cloud_policy_store.h"
#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#endif

namespace policy {
#if defined(OS_WIN)
DomainEnrollmentStatusProvider::DomainEnrollmentStatusProvider() = default;

DomainEnrollmentStatusProvider::~DomainEnrollmentStatusProvider() = default;

bool DomainEnrollmentStatusProvider::IsManaged() {
  return DomainEnrollmentStatusProvider::IsEnrolledToDomain();
}

EnterpriseManagementAuthority DomainEnrollmentStatusProvider::GetAuthority() {
  return EnterpriseManagementAuthority::DOMAIN_LOCAL;
}

bool DomainEnrollmentStatusProvider::IsEnrolledToDomain() {
  return base::win::IsEnrolledToDomain();
}
#endif

EnterpriseMDMManagementStatusProvider::EnterpriseMDMManagementStatusProvider() =
    default;

EnterpriseMDMManagementStatusProvider::
    ~EnterpriseMDMManagementStatusProvider() = default;

bool EnterpriseMDMManagementStatusProvider::IsManaged() {
#if defined(OS_WIN)
  return base::win::OSInfo::GetInstance()->version_type() !=
             base::win::SUITE_HOME &&
         base::win::IsDeviceRegisteredWithManagement();
#endif
  return false;
}

EnterpriseManagementAuthority
EnterpriseMDMManagementStatusProvider::GetAuthority() {
  return EnterpriseManagementAuthority::CLOUD;
}

}  // namespace policy
