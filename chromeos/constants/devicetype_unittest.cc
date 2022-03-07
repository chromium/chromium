// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
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
    command_line->InitFromArgv({"", ""});
    EXPECT_EQ(chromeos::GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(DeviceTypeTest, GetDeviceTypeLacros) {
  base::test::TaskEnvironment task_environment;
  chromeos::ScopedDisableCrosapiForTesting disable_crosapi;
  chromeos::LacrosService lacros_service;
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebook;
    lacros_service.SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebook);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebase;
    lacros_service.SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebase);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebit;
    lacros_service.SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebit);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kChromebox;
    lacros_service.SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kChromebox);
  }
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->device_type =
        crosapi::mojom::BrowserInitParams::DeviceType::kUnknown;
    lacros_service.SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
  {
    // When device_type is not set, GetDeviceType() should return kUnknown.
    auto params = crosapi::mojom::BrowserInitParams::New();
    lacros_service.SetInitParamsForTests(std::move(params));
    EXPECT_EQ(GetDeviceType(), chromeos::DeviceType::kUnknown);
  }
}
#endif

}  // namespace chromeos
