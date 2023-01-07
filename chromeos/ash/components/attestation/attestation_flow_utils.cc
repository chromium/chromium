// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_utils.h"

#include <string>

#include "base/notreached.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

namespace ash {
namespace attestation {

std::string GetKeyNameForProfile(
    AttestationCertificateProfile certificate_profile,
    const std::string& request_origin) {
  switch (certificate_profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
      return kEnterpriseMachineKey;
    case PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE:
      return kEnterpriseEnrollmentKey;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
      return kEnterpriseUserKey;
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return std::string(kContentProtectionKeyPrefix) + request_origin;
    case PROFILE_SOFT_BIND_CERTIFICATE:
      return kSoftBindKey;
    case PROFILE_DEVICE_SETUP_CERTIFICATE:
      return kDeviceSetupKey;
  }
  NOTREACHED();
  return "";
}

}  // namespace attestation
}  // namespace ash
