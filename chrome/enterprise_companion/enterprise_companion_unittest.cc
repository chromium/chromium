// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

TEST(EnterpriseCompanionTest, AppExitCode) {
  ASSERT_EQ(EnterpriseCompanionMain(0, {}), 0);
}

}  // namespace enterprise_companion
