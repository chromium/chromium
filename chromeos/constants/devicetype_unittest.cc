// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/devicetype.h"

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(DeviceTypeTest, GetDeviceTypeAsh) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  {
    command_line->InitFromArgv({"", "--form-factor=CHROMEBOOK"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=CHROMESLATE"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=CLAMSHELL"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=CONVERTIBLE"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=DETACHABLE"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=CHROMEBASE"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebase);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=CHROMEBIT"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebit);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=CHROMEBOX"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kChromebox);
  }
  {
    command_line->InitFromArgv({"", "--form-factor=OTHER"});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
  {
    command_line->InitFromArgv({"", ""});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
}

}  // namespace chromeos
