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

void EnsureExtensionsClientInitialized() {
  static bool initialized = false;

  static base::NoDestructor<extensions::ChromeExtensionsClient>
      extensions_client;

  if (!initialized) {
    initialized = true;
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
