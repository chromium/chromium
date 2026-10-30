// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/conditional_skipping.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "build/build_config.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "device/vr/windows/d3d11_device_helpers.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(SUPPORTS_OZONE_X11)
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#endif

namespace vr {

std::string CheckDirectX_11_1() {
#if BUILDFLAG(IS_WIN)
  int32_t adapter_index;
  GetD3D11_1AdapterIndex(&adapter_index);
  if (adapter_index == -1) {
    return "DirectX 11.1 required, but no suitable device found";
  }
  return "";
#else
  return "DirectX 11.1 required, but not on Windows";
#endif  // BUILDFLAG(IS_WIN)
}

std::string CheckDri3() {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(SUPPORTS_OZONE_X11)
  // The GPU process needs DRI3 to share WebXR frames as DMA-BUFs.
  x11::Connection connection;
  if (connection.Ready() && connection.dri3().present()) {
    return "";
  }
  return "DRI3 required, but not supported by the X server";
#else
  return "DRI3 required, but not on Linux with X11";
#endif
}

std::string CheckXrRequirements(
    const std::vector<XrTestRequirement>& requirements_vector,
    const std::unordered_set<std::string>& ignored_set) {
  if (ignored_set.count("*") == 1) {
    LOG(WARNING) << "Skipping all runtime requirement checks due to wildcard "
                 << "in set of requirements to ignore";
    return "";
  }
  for (auto requirement : requirements_vector) {
    if (ignored_set.count(XrTestRequirementToString(requirement)) == 1) {
      LOG(WARNING) << "Skipped checking runtime requirement "
                   << XrTestRequirementToString(requirement)
                   << " due to it being in the set of requirements to ignore";
      continue;
    }
    std::string ret;
    switch (requirement) {
      case XrTestRequirement::DIRECTX_11_1:
        ret = CheckDirectX_11_1();
        break;
      case XrTestRequirement::DRI3:
        ret = CheckDri3();
        break;
    }
    if (ret != "")
      return ret;
  }
  return "";
}

std::string XrTestRequirementToString(XrTestRequirement requirement) {
  switch (requirement) {
    case XrTestRequirement::DIRECTX_11_1:
      return "DirectX_11.1";
    case XrTestRequirement::DRI3:
      return "DRI3";
    default:
      return "Unknown requirement";
  }
}

}  // namespace vr
