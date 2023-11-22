// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/cros_safe_seed_manager.h"

#include "components/variations/service/safe_seed_manager_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::cros_early_boot::evaluate_seed {

TEST(CrosSafeSeedManagerTest, GetSeedType_RegularSeed) {
  CrOSSafeSeedManager safe_seed_manager(SeedType::kRegularSeed);

  EXPECT_EQ(SeedType::kRegularSeed, safe_seed_manager.GetSeedType());
}

TEST(CrosSafeSeedManagerTest, GetSeedType_SafeSeed) {
  CrOSSafeSeedManager safe_seed_manager(SeedType::kSafeSeed);

  EXPECT_EQ(SeedType::kSafeSeed, safe_seed_manager.GetSeedType());
}

}  // namespace variations::cros_early_boot::evaluate_seed
