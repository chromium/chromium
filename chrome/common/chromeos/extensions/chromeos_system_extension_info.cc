// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {

namespace switches {

// Overrides |manufacturer| field of the ChromeOSSystemExtensionInfo structure.
// Used for development/testing.
const char kTelemetryExtensionManufacturerOverrideForTesting[] =
    "telemetry-extension-manufacturer-override-for-testing";

// Overrides |pwa_origin| field of the ChromeOSSystemExtensionInfo structure.
// Used for development/testing.
const char kTelemetryExtensionPwaOriginOverrideForTesting[] =
    "telemetry-extension-pwa-origin-override-for-testing";

// Overrides |iwa_id| field of the ChromeOSSystemExtensionInfo structure.
// Used for development/testing.
const char kTelemetryExtensionIwaIdOverrideForTesting[] =
    "telemetry-extension-iwa-id-override-for-testing";

}  // namespace switches

namespace {

using ChromeOSSystemExtensionInfoMap =
    base::flat_map<std::string, ChromeOSSystemExtensionInfo>;

ChromeOSSystemExtensionInfoMap ConstructMap() {
  ChromeOSSystemExtensionInfoMap map{
      {/*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
       {
           /*manufacturers=*/{"HP", "ASUS"},
           /*pwa_origin=*/"*://googlechromelabs.github.io/*",
           /*iwa_id=*/std::nullopt,
       }},
      {/*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
       {
           /*manufacturers=*/{"HP"},
           /*pwa_origin=*/"https://hpcs-appschr.hpcloud.hp.com/*",
           /*iwa_id=*/std::nullopt,
       }},
      {/*extension_id=*/"hdnhcpcfohaeangjpkcjkgmgmjanbmeo",
       {
           /*manufacturers=*/{"ASUS"},
           /*pwa_origin=*/"https://dlcdnccls.asus.com/*",
           /*iwa_id=*/std::nullopt,
       }},
  };

  if (IsChromeOSSystemExtensionDevExtensionEnabled()) {
    map.try_emplace(
        kChromeOSSystemExtensionDevExtensionId,
        ChromeOSSystemExtensionInfo{
            /*manufacturers=*/{"Google", "HP", "ASUS"},
            /*pwa_origin=*/"*://googlechromelabs.github.io/*",
            /*iwa_id=*/
            web_package::SignedWebBundleId::Create(
                "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic")
                .value(),
        });
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kTelemetryExtensionPwaOriginOverrideForTesting)) {
    auto pwa_origin = command_line->GetSwitchValueASCII(
        switches::kTelemetryExtensionPwaOriginOverrideForTesting);
    for (auto& [extension_id, extension_info] : map) {
      extension_info.pwa_origin = pwa_origin;
    }
  }

  if (command_line->HasSwitch(
          switches::kTelemetryExtensionManufacturerOverrideForTesting)) {
    auto manufacturer = command_line->GetSwitchValueASCII(
        switches::kTelemetryExtensionManufacturerOverrideForTesting);
    for (auto& [extension_id, extension_info] : map) {
      extension_info.manufacturers = {manufacturer};
    }
  }

  if (command_line->HasSwitch(
          switches::kTelemetryExtensionIwaIdOverrideForTesting)) {
    auto iwa_id_str = command_line->GetSwitchValueASCII(
        switches::kTelemetryExtensionIwaIdOverrideForTesting);
    auto iwa_id_res = web_package::SignedWebBundleId::Create(iwa_id_str);
    if (iwa_id_res.has_value()) {
      for (auto& [extension_id, extension_info] : map) {
        extension_info.iwa_id = iwa_id_res.value();
      }
    } else {
      LOG(ERROR) << "Failed to parse iwa id " << iwa_id_str << ": "
                 << iwa_id_res.error();
    }
  }

  return map;
}

ChromeOSSystemExtensionInfoMap*& GetMap() {
  static ChromeOSSystemExtensionInfoMap* g_map =
      new ChromeOSSystemExtensionInfoMap{ConstructMap()};
  return g_map;
}

}  // namespace

bool IsChromeOSSystemExtensionDevExtensionEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::features::IsShimlessRMA3pDiagnosticsDevModeEnabled();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ChromeOSSystemExtensionInfo::ChromeOSSystemExtensionInfo(
    const base::flat_set<std::string>& manufacturers,
    const std::optional<std::string>& pwa_origin,
    const std::optional<web_package::SignedWebBundleId>& iwa_id)
    : manufacturers(manufacturers), pwa_origin(pwa_origin), iwa_id(iwa_id) {}

ChromeOSSystemExtensionInfo::ChromeOSSystemExtensionInfo(
    const ChromeOSSystemExtensionInfo&) = default;

ChromeOSSystemExtensionInfo& ChromeOSSystemExtensionInfo::operator=(
    const ChromeOSSystemExtensionInfo&) = default;

ChromeOSSystemExtensionInfo::~ChromeOSSystemExtensionInfo() = default;

const ChromeOSSystemExtensionInfo& GetChromeOSExtensionInfoById(
    const std::string& id) {
  CHECK(IsChromeOSSystemExtension(id));
  return GetMap()->at(id);
}

bool IsChromeOSSystemExtension(const std::string& id) {
  return GetMap()->find(id) != GetMap()->end();
}

bool Is3pDiagnosticsIwaId(const web_package::SignedWebBundleId& id) {
  for (const auto& [extension_id, extension_info] : *GetMap()) {
    if (extension_info.iwa_id == id) {
      return true;
    }
  }
  return false;
}

bool IsChromeOSSystemExtensionProvider(const std::string& manufacturer) {
  for (const auto& [extension_id, info] : *GetMap()) {
    if (info.manufacturers.contains(manufacturer)) {
      return true;
    }
  }
  return false;
}

class ScopedChromeOSSystemExtensionInfoImpl
    : public ScopedChromeOSSystemExtensionInfo {
 public:
  ScopedChromeOSSystemExtensionInfoImpl();
  ScopedChromeOSSystemExtensionInfoImpl(
      ScopedChromeOSSystemExtensionInfoImpl&) = delete;
  ScopedChromeOSSystemExtensionInfoImpl& operator=(
      ScopedChromeOSSystemExtensionInfoImpl&) = delete;
  ~ScopedChromeOSSystemExtensionInfoImpl() override;

  void ApplyCommandLineSwitchesForTesting() override;  // IN-TEST

 private:
  raw_ptr<ChromeOSSystemExtensionInfoMap> map_;
};

// static
std::unique_ptr<ScopedChromeOSSystemExtensionInfo>
ScopedChromeOSSystemExtensionInfo::CreateForTesting() {
  return std::make_unique<ScopedChromeOSSystemExtensionInfoImpl>();
}

ScopedChromeOSSystemExtensionInfoImpl::ScopedChromeOSSystemExtensionInfoImpl() {
  map_ = GetMap();
  GetMap() = new ChromeOSSystemExtensionInfoMap{ConstructMap()};
}

ScopedChromeOSSystemExtensionInfoImpl::
    ~ScopedChromeOSSystemExtensionInfoImpl() {
  delete GetMap();
  GetMap() = map_;
}

void ScopedChromeOSSystemExtensionInfoImpl::
    ApplyCommandLineSwitchesForTesting() {
  delete GetMap();
  GetMap() = new ChromeOSSystemExtensionInfoMap{ConstructMap()};
}

}  // namespace chromeos
