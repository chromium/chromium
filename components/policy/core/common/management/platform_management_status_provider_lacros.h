// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_LACROS_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_LACROS_H_

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

// Returns the platform managment status of LaCros devices. For Chrome OS Ash,
// see DeviceManagementStatusProvider which needs to be defined under
// c/browser/enterprise/browser_management/browser_management_status_provider.
class POLICY_EXPORT DeviceEnterpriseManagedStatusProvider final
    : public ManagementStatusProvider {
 public:
  DeviceEnterpriseManagedStatusProvider();

  DeviceEnterpriseManagedStatusProvider(
      const DeviceEnterpriseManagedStatusProvider&) = delete;
  DeviceEnterpriseManagedStatusProvider& operator=(
      const DeviceEnterpriseManagedStatusProvider&) = delete;

 protected:
  // ManagementStatusProvider impl
  EnterpriseManagementAuthority FetchAuthority() final;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_LACROS_H_
