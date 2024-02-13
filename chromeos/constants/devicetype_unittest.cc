// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/devicetype.h"

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace chromeos {

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(DeviceTypeTest, GetDeviceTypeLacros) {
  base::test::TaskEnvironment task_environment;
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebook;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebase;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebase);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebit;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebit);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebox;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebox);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kUnknown;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
  {
    // When device_type is not set, GetDeviceType() should return kUnknown.
    auto params = crosapi::mojom::BrowserInitParams::New();
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
}
#endif

}  // namespace chromeos
