// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_mac.h"

#include "components/policy/core/common/policy_pref_names.h"

namespace policy {

DomainEnrollmentStatusProvider::DomainEnrollmentStatusProvider() {
  domain_join_state_ = base::AreDeviceAndUserJoinedToDomain();
}

DomainEnrollmentStatusProvider::~DomainEnrollmentStatusProvider() = default;

EnterpriseManagementAuthority DomainEnrollmentStatusProvider::FetchAuthority() {
  return domain_join_state_.device_joined || domain_join_state_.user_joined
             ? EnterpriseManagementAuthority::DOMAIN_LOCAL
             : EnterpriseManagementAuthority::NONE;
}

EnterpriseMDMManagementStatusProvider::EnterpriseMDMManagementStatusProvider()
    : ManagementStatusProvider(policy_prefs::kEnterpriseMDMManagementMac) {}

EnterpriseMDMManagementStatusProvider::
    ~EnterpriseMDMManagementStatusProvider() = default;

EnterpriseManagementAuthority
EnterpriseMDMManagementStatusProvider::FetchAuthority() {
  base::MacDeviceManagementStateNew mdm_state_new =
      base::IsDeviceRegisteredWithManagementNew();

  bool managed = false;
  switch (mdm_state_new) {
    case base::MacDeviceManagementStateNew::kLimitedMDMEnrollment:
    case base::MacDeviceManagementStateNew::kFullMDMEnrollment:
    case base::MacDeviceManagementStateNew::kDEPMDMEnrollment:
      managed = true;
      break;
    case base::MacDeviceManagementStateNew::kFailureAPIUnavailable:
      managed = base::MacDeviceManagementStateOld::kMDMEnrollment ==
                base::IsDeviceRegisteredWithManagementOld();
      break;
    default:
      break;
  }

  return managed ? EnterpriseManagementAuthority::CLOUD
                 : EnterpriseManagementAuthority::NONE;
}

}  // namespace policy
