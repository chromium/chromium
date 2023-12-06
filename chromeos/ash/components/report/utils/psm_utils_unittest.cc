// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/psm_utils.h"

#include "chromeos/ash/components/report/utils/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::utils {

TEST(PsmUtilsTest, GeneratePsmIdentifier) {
  std::string psm_use_case_str =
      psm_rlwe::RlweUseCase_Name(psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY);
  std::string window_id = "20230101";

  std::optional<psm_rlwe::RlwePlaintextId> psm_identifier =
      GeneratePsmIdentifier(utils::kFakeHighEntropySeed, psm_use_case_str,
                            window_id);

  EXPECT_TRUE(psm_identifier.has_value());
  EXPECT_FALSE(psm_identifier->sensitive_id().empty());

  // Validate fake sensitive id is generated consistently and doesn't regress.
  EXPECT_EQ(psm_identifier->sensitive_id(),
            "DD2FEE247A10674D6C58835BDC07553B820F741615F32E5EA908E62E245B529B");
}

TEST(PsmUtilsTest, GeneratePsmIdentifierMissingParameters) {
  // Test case for GeneratePsmIdentifier with missing parameters.
  std::string psm_use_case_str =
      psm_rlwe::RlweUseCase_Name(psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY);
  std::string window_id = "20230101";

  // Attempt to generate PSM identifiers with various missing parameters.
  std::optional<psm_rlwe::RlwePlaintextId> psm_identifier_missing_parameters =
      GeneratePsmIdentifier(std::string(), std::string(), std::string());
  std::optional<psm_rlwe::RlwePlaintextId> psm_identifier_no_window_id =
      GeneratePsmIdentifier(utils::kFakeHighEntropySeed, psm_use_case_str,
                            std::string());
  std::optional<psm_rlwe::RlwePlaintextId> psm_identifier_no_use_case =
      GeneratePsmIdentifier(utils::kFakeHighEntropySeed, std::string(),
                            window_id);
  std::optional<psm_rlwe::RlwePlaintextId> psm_identifier_no_seed =
      GeneratePsmIdentifier(std::string(), psm_use_case_str, window_id);

  EXPECT_FALSE(psm_identifier_missing_parameters.has_value());
  EXPECT_FALSE(psm_identifier_no_window_id.has_value());
  EXPECT_FALSE(psm_identifier_no_use_case.has_value());
  EXPECT_FALSE(psm_identifier_no_seed.has_value());
}

}  // namespace ash::report::utils
