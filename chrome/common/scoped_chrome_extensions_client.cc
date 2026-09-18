// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/scoped_chrome_extensions_client.h"

#include <map>
#include <memory>

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/common/controlled_frame/controlled_frame.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mime_handler_availability.h"
#include "extensions/common/user_scripts_availability.h"
#include "extensions/common/webstore_override.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/controlled_frame/controlled_frame_api_provider.h"
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/common/apps/platform_apps/chrome_apps_api_provider.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_provider.h"
#include "chromeos/ash/experiences/extensions/common/chromeos_extensions_api_provider.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Helper method to merge all the FeatureDelegatedAvailabilityCheckMaps into a
// single map.
Feature::FeatureDelegatedAvailabilityCheckMap
CombineAllAvailabilityCheckMaps() {
  Feature::FeatureDelegatedAvailabilityCheckMap map_list[] = {
      controlled_frame::CreateAvailabilityCheckMap(),
      mime_handler_availability::CreateAvailabilityCheckMap(),
      user_scripts_availability::CreateAvailabilityCheckMap(),
      webstore_override::CreateAvailabilityCheckMap(),
  };
  Feature::FeatureDelegatedAvailabilityCheckMap result;

  for (auto& map : map_list) {
    result.merge(map);
    // Overlapping names could silently install the wrong availability policy.
    CHECK(map.empty()) << "Overlapping delegated availability handler for: "
                       << map.begin()->first;
  }
  return result;
}

}  // namespace

ScopedChromeExtensionsClient::ScopedChromeExtensionsClient()
    : client_(std::make_unique<ChromeExtensionsClient>()) {
  client_->SetFeatureDelegatedAvailabilityCheckMap(
      CombineAllAvailabilityCheckMaps());
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  client_->AddAPIProvider(
      std::make_unique<chrome_apps::ChromeAppsAPIProvider>());
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  client_->AddAPIProvider(
      std::make_unique<controlled_frame::ControlledFrameAPIProvider>());
#endif
#if BUILDFLAG(IS_CHROMEOS)
  client_->AddAPIProvider(
      std::make_unique<ash::ChromeOSExtensionsAPIProvider>());
  client_->AddAPIProvider(
      std::make_unique<chromeos::ChromeOSSystemExtensionsAPIProvider>());
#endif
  ExtensionsClient::Set(client_.get());
}

ScopedChromeExtensionsClient::~ScopedChromeExtensionsClient() {
  DCHECK_EQ(ExtensionsClient::Get(), client_.get());
  ExtensionsClient::Set(nullptr);
}

}  // namespace extensions
