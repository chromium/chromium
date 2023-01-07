// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_win.h"

#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"

namespace policy {
DomainEnrollmentStatusProvider::DomainEnrollmentStatusProvider() = default;

EnterpriseManagementAuthority DomainEnrollmentStatusProvider::FetchAuthority() {
  return DomainEnrollmentStatusProvider::IsEnrolledToDomain() ? DOMAIN_LOCAL
                                                              : NONE;
}

bool DomainEnrollmentStatusProvider::IsEnrolledToDomain() {
  return base::win::IsEnrolledToDomain();
}

EnterpriseMDMManagementStatusProvider::EnterpriseMDMManagementStatusProvider()
    : ManagementStatusProvider(policy_prefs::kEnterpriseMDMManagementWindows) {}

EnterpriseManagementAuthority
EnterpriseMDMManagementStatusProvider::FetchAuthority() {
  return base::win::IsDeviceRegisteredWithManagement() ? CLOUD : NONE;
}

AzureActiveDirectoryStatusProvider::AzureActiveDirectoryStatusProvider()
    : ManagementStatusProvider(policy_prefs::kAzureActiveDirectoryManagement) {}

EnterpriseManagementAuthority
AzureActiveDirectoryStatusProvider::FetchAuthority() {
  return base::win::IsJoinedToAzureAD() ? CLOUD_DOMAIN : NONE;
}

}  // namespace policy
