// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_MAC_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_MAC_H_

#include "base/enterprise_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class POLICY_EXPORT DomainEnrollmentStatusProvider
    : public ManagementStatusProvider {
 public:
  DomainEnrollmentStatusProvider();
  ~DomainEnrollmentStatusProvider() final;

  // ManagementStatusProvider impl
  bool IsManaged() final;
  EnterpriseManagementAuthority GetAuthority() final;

 private:
  base::DeviceUserDomainJoinState domain_join_state_;
};

class POLICY_EXPORT EnterpriseMDMManagementStatusProvider
    : public ManagementStatusProvider {
 public:
  EnterpriseMDMManagementStatusProvider();
  ~EnterpriseMDMManagementStatusProvider() final;

  // ManagementStatusProvider impl
  bool IsManaged() final;
  EnterpriseManagementAuthority GetAuthority() final;

 private:
  base::MacDeviceManagementStateOld mdm_state_old_ =
      base::MacDeviceManagementStateOld::kFailureAPIUnavailable;
  base::MacDeviceManagementStateNew mdm_state_new_ =
      base::MacDeviceManagementStateNew::kFailureAPIUnavailable;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_MAC_H_
