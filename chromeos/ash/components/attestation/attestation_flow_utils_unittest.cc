// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_utils.h"

#include <string>

#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {

namespace {

constexpr char kFakeOrigin[] = "origin";

}  // namespace

TEST(AttestationFlowUtilsTest, GetKeyNameForProfile) {
  EXPECT_EQ(
      GetKeyNameForProfile(PROFILE_ENTERPRISE_MACHINE_CERTIFICATE, kFakeOrigin),
      kEnterpriseMachineKey);
  EXPECT_EQ(GetKeyNameForProfile(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
                                 kFakeOrigin),
            kEnterpriseEnrollmentKey);
  EXPECT_EQ(
      GetKeyNameForProfile(PROFILE_ENTERPRISE_USER_CERTIFICATE, kFakeOrigin),
      kEnterpriseUserKey);
  EXPECT_EQ(
      GetKeyNameForProfile(PROFILE_CONTENT_PROTECTION_CERTIFICATE, kFakeOrigin),
      std::string(kContentProtectionKeyPrefix) + kFakeOrigin);
}

}  // namespace attestation
}  // namespace ash
