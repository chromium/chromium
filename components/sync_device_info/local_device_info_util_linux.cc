// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // for HOST_NAME_MAX
#include <unistd.h>  // for gethostname()

#include <string>

#include "base/linux_util.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/constants/devicetype.h"  // nogncheck
#endif

namespace syncer {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string GetChromeOSDeviceNameFromType() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
      return "Chromebase";
    case chromeos::DeviceType::kChromebit:
      return "Chromebit";
    case chromeos::DeviceType::kChromebook:
      return "Chromebook";
    case chromeos::DeviceType::kChromebox:
      return "Chromebox";
    case chromeos::DeviceType::kUnknown:
      break;
  }
  return "Chromebook";
}
#endif

std::string GetPersonalizableDeviceNameInternal() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return GetChromeOSDeviceNameFromType();
#else
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0) {  // Success.
    return hostname;
  }
  return base::GetLinuxDistro();
#endif
}

}  // namespace syncer
