// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

TEST(Util, AppArgsAndAP) {
  base::test::ScopedCommandLine original_command_line;
  {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(kTagSwitch,
                                    "appguid=8a69f345-c564-463c-aff1-"
                                    "a69d9e530f96&appname=TestApp&ap=TestAP");

    // Test GetAppArgs.
    EXPECT_EQ(GetAppArgs("NonExistentAppId"), absl::nullopt);
    absl::optional<tagging::AppArgs> app_args =
        GetAppArgs("8a69f345-c564-463c-aff1-a69d9e530f96");
    ASSERT_NE(app_args, absl::nullopt);
    EXPECT_STREQ(app_args->app_id.c_str(),
                 "8a69f345-c564-463c-aff1-a69d9e530f96");
    EXPECT_STREQ(app_args->app_name.c_str(), "TestApp");

    // Test GetAPFromAppArgs.
    EXPECT_STREQ(GetAPFromAppArgs("NonExistentAppId").c_str(), "");
    EXPECT_STREQ(
        GetAPFromAppArgs("8a69f345-c564-463c-aff1-a69d9e530f96").c_str(),
        "TestAP");
  }
}

}  // namespace updater
