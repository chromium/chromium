// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/devicetype.h"

#include "base/logging.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"  // nogncheck
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos {

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kDeviceTypeKey[] = "DEVICETYPE";
constexpr char kFormFactor[] = "form-factor";
#endif
}  // namespace

DeviceType GetDeviceType() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string value;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kFormFactor)) {
    value = command_line->GetSwitchValueASCII(kFormFactor);
  } else if (!base::SysInfo::GetLsbReleaseValue(kDeviceTypeKey, &value)) {
    return DeviceType::kUnknown;
  }

  // Most devices are Chromebooks, so we will also consider reference boards
  // as Chromebooks.
  if (value == "CHROMEBOOK" || value == "REFERENCE" || value == "CHROMESLATE" ||
      value == "CLAMSHELL" || value == "CONVERTIBLE" || value == "DETACHABLE")
    return DeviceType::kChromebook;
  if (value == "CHROMEBASE")
    return DeviceType::kChromebase;
  if (value == "CHROMEBIT")
    return DeviceType::kChromebit;
  if (value == "CHROMEBOX")
    return DeviceType::kChromebox;
  // Don't log errors for VMs, which are type "OTHER".
  if (value == "OTHER") {
    return DeviceType::kUnknown;
  }
  LOG(ERROR) << "Unknown device type \"" << value << "\"";
  return DeviceType::kUnknown;

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto device_type = BrowserParamsProxy::Get()->DeviceType();
  switch (device_type) {
    case crosapi::mojom::BrowserInitParams::DeviceType::kChromebook:
      return chromeos::DeviceType::kChromebook;
    case crosapi::mojom::BrowserInitParams::DeviceType::kChromebase:
      return chromeos::DeviceType::kChromebase;
    case crosapi::mojom::BrowserInitParams::DeviceType::kChromebit:
      return chromeos::DeviceType::kChromebit;
    case crosapi::mojom::BrowserInitParams::DeviceType::kChromebox:
      return chromeos::DeviceType::kChromebox;
    case crosapi::mojom::BrowserInitParams::DeviceType::kUnknown:
      [[fallthrough]];
    default:
      return chromeos::DeviceType::kUnknown;
  }
#endif
}

}  // namespace chromeos
