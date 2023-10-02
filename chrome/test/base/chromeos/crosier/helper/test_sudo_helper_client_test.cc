// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(TestSudoHelperClientTest, WhoAmI) {
  auto result = TestSudoHelperClient().RunCommand("whoami");
  EXPECT_EQ(result.return_code, 0);
  EXPECT_EQ(base::TrimString(result.output, " \n", base::TRIM_ALL), "root");
}
