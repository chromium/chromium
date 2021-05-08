// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_mac.h"

namespace policy {

DomainEnrollmentStatusProvider::DomainEnrollmentStatusProvider() {
  domain_join_state_ = base::AreDeviceAndUserJoinedToDomain();
}

DomainEnrollmentStatusProvider::~DomainEnrollmentStatusProvider() = default;

bool DomainEnrollmentStatusProvider::IsManaged() {
  return domain_join_state_.device_joined || domain_join_state_.user_joined;
}

EnterpriseManagementAuthority DomainEnrollmentStatusProvider::GetAuthority() {
  return EnterpriseManagementAuthority::DOMAIN_LOCAL;
}

EnterpriseMDMManagementStatusProvider::EnterpriseMDMManagementStatusProvider() {
  mdm_state_new_ = base::IsDeviceRegisteredWithManagementNew();
  if (mdm_state_new_ ==
      base::MacDeviceManagementStateNew::kFailureAPIUnavailable) {
    mdm_state_old_ = base::IsDeviceRegisteredWithManagementOld();
  }
}

EnterpriseMDMManagementStatusProvider::
    ~EnterpriseMDMManagementStatusProvider() = default;

bool EnterpriseMDMManagementStatusProvider::IsManaged() {
  if (mdm_state_new_ ==
      base::MacDeviceManagementStateNew::kFailureAPIUnavailable) {
    return mdm_state_old_ == base::MacDeviceManagementStateOld::kMDMEnrollment;
  }

  return mdm_state_new_ ==
             base::MacDeviceManagementStateNew::kLimitedMDMEnrollment ||
         mdm_state_new_ ==
             base::MacDeviceManagementStateNew::kFullMDMEnrollment ||
         mdm_state_new_ == base::MacDeviceManagementStateNew::kDEPMDMEnrollment;
}

EnterpriseManagementAuthority
EnterpriseMDMManagementStatusProvider::GetAuthority() {
  return EnterpriseManagementAuthority::CLOUD;
}

}  // namespace policy
