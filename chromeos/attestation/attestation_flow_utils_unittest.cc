// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow_utils.h"

#include <string>

#include "chromeos/dbus/constants/attestation_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
}  // namespace chromeos
