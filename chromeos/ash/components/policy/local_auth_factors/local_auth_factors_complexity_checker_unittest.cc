// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity_checker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(LocalAuthFactorsComplexityChecker, Password) {
  EXPECT_TRUE(
      LocalAuthFactorsComplexityChecker::CheckPasswordComplexity("password"));
}

TEST(LocalAuthFactorsComplexityChecker, Pin) {
  EXPECT_TRUE(LocalAuthFactorsComplexityChecker::CheckPinComplexity("1234"));
}

}  // namespace policy
