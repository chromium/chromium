// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/initialize_extensions_client.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/apps/platform_apps/chrome_apps_api_provider.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "extensions/common/extensions_client.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_provider.h"
#endif

// This list should stay in sync with GetExpectedDelegatedFeaturesForTest().
base::span<const char* const> GetControlledFrameFeatureList() {
  constexpr const char* feature_list[] = {
      "chromeWebViewInternal", "declarativeWebRequest",
      "guestViewInternal",     "webRequest",
      "webViewInternal",       "webViewRequest",
  };
  return base::make_span(feature_list);
}

void EnsureExtensionsClientInitialized() {
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap map;
  EnsureExtensionsClientInitialized(std::move(map));
}

void EnsureExtensionsClientInitialized(
    extensions::Feature::FeatureDelegatedAvailabilityCheckMap
        delegated_availability_map) {
  static bool initialized = false;

  static base::NoDestructor<extensions::ChromeExtensionsClient>
      extensions_client;

  if (!initialized) {
    initialized = true;
    extensions_client->SetFeatureDelegatedAvailabilityCheckMap(
        std::move(delegated_availability_map));
    extensions_client->AddAPIProvider(
        std::make_unique<chrome_apps::ChromeAppsAPIProvider>());
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
