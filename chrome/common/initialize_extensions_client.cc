// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/initialize_extensions_client.h"

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/webstore_override.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/controlled_frame/controlled_frame.h"
#include "chrome/common/controlled_frame/controlled_frame_api_provider.h"
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/common/apps/platform_apps/chrome_apps_api_provider.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_provider.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Helper method to merge all the FeatureDelegatedAvailabilityCheckMaps into a
// single map.
extensions::Feature::FeatureDelegatedAvailabilityCheckMap
CombineAllAvailabilityCheckMaps() {
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap map_list[] = {
      controlled_frame::CreateAvailabilityCheckMap(),
      extensions::webstore_override::CreateAvailabilityCheckMap()};
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap result;

  for (auto& map : map_list) {
    result.merge(map);
    // DCHECK that none of the keys were overlapping i.e. the map we merged in
    // is empty now. This is done as a DCHECK rather than a CHECK as it is meant
    // as a catch for developers adding a new delegated availability check that
    // might have overlapping keys with an existing one.
    DCHECK(map.empty())
        << "Overlapping feature name key in delegated availibty check map for: "
        << map.begin()->first;
  }
  return result;
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

void EnsureExtensionsClientInitialized() {
  static bool initialized = false;

  static base::NoDestructor<extensions::ChromeExtensionsClient>
      extensions_client;

  if (!initialized) {
    initialized = true;

#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions_client->SetFeatureDelegatedAvailabilityCheckMap(
        CombineAllAvailabilityCheckMaps());
#endif
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
    extensions_client->AddAPIProvider(
        std::make_unique<chrome_apps::ChromeAppsAPIProvider>());
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions_client->AddAPIProvider(
        std::make_unique<controlled_frame::ControlledFrameAPIProvider>());
#endif
#if BUILDFLAG(IS_CHROMEOS)
    extensions_client->AddAPIProvider(
        std::make_unique<chromeos::ChromeOSSystemExtensionsAPIProvider>());
#endif
    extensions::ExtensionsClient::Set(extensions_client.get());
  }

  // ExtensionsClient::Set() will early-out if the client was already set, so
  // this allows us to check that this was the only site setting it.
  DCHECK_EQ(extensions_client.get(), extensions::ExtensionsClient::Get())
      << "ExtensionsClient should only be initialized through "
      << "EnsureExtensionsClientInitialized() when using "
      << "ChromeExtensionsClient.";
}
