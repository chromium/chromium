// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/run_on_os_login_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using RunOnOsLoginTest = testing::Test;

TEST_F(RunOnOsLoginTest, VerifyRunOnOsLoginConvert) {
  {
    // Verify the convert function can work for null permission.
    EXPECT_FALSE(ConvertDictToRunOnOsLogin(nullptr).has_value());
  }

  {
    RunOnOsLogin run_on_os_login(RunOnOsLoginMode::kNotRun,
                                 /*is_managed=*/true);
    auto dict = ConvertRunOnOsLoginToDict(run_on_os_login);
    EXPECT_EQ(run_on_os_login, ConvertDictToRunOnOsLogin(&dict).value());
  }

  {
    RunOnOsLogin run_on_os_login(RunOnOsLoginMode::kWindowed,
                                 /*is_managed=*/false);
    auto dict = ConvertRunOnOsLoginToDict(run_on_os_login);
    EXPECT_EQ(run_on_os_login, ConvertDictToRunOnOsLogin(&dict).value());
  }
}

}  // namespace apps
