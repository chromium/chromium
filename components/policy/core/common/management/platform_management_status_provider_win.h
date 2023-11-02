// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_WIN_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_WIN_H_

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class POLICY_EXPORT DomainEnrollmentStatusProvider final
    : public ManagementStatusProvider {
 public:
  DomainEnrollmentStatusProvider();

  DomainEnrollmentStatusProvider(const DomainEnrollmentStatusProvider&) =
      delete;
  DomainEnrollmentStatusProvider& operator=(
      const DomainEnrollmentStatusProvider&) = delete;

  static bool IsEnrolledToDomain();

 protected:
  // ManagementStatusProvider impl
  EnterpriseManagementAuthority FetchAuthority() final;
};

class POLICY_EXPORT EnterpriseMDMManagementStatusProvider final
    : public ManagementStatusProvider {
 public:
  EnterpriseMDMManagementStatusProvider();

  EnterpriseMDMManagementStatusProvider(
      const EnterpriseMDMManagementStatusProvider&) = delete;
  EnterpriseMDMManagementStatusProvider& operator=(
      const EnterpriseMDMManagementStatusProvider&) = delete;

  static bool IsEnrolledToDomain();

 protected:
  // ManagementStatusProvider impl
  EnterpriseManagementAuthority FetchAuthority() final;
};

// TODO (crbug/1300217): Handle management state changing while the browser is
// running.
class POLICY_EXPORT AzureActiveDirectoryStatusProvider final
    : public ManagementStatusProvider {
 public:
  AzureActiveDirectoryStatusProvider();

  AzureActiveDirectoryStatusProvider(
      const AzureActiveDirectoryStatusProvider&) = delete;
  AzureActiveDirectoryStatusProvider& operator=(
      const AzureActiveDirectoryStatusProvider&) = delete;

 protected:
  // ManagementStatusProvider impl
  EnterpriseManagementAuthority FetchAuthority() override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_WIN_H_
