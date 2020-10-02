// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

bool IsExtensionBlockedByPolicy(content::BrowserContext* context,
                                const std::string& extension_id) {
  auto* registry = ExtensionRegistry::Get(context);
  // May be nullptr in unit tests.
  if (!registry)
    return false;

  const Extension* extension = registry->GetInstalledExtension(extension_id);
  ExtensionManagement* management =
      ExtensionManagementFactory::GetForBrowserContext(context);
  ExtensionManagement::InstallationMode mode =
      extension ? management->GetInstallationMode(extension)
                : management->GetInstallationMode(extension_id,
                                                  /*update_url=*/std::string());
  return mode == ExtensionManagement::INSTALLATION_BLOCKED ||
         mode == ExtensionManagement::INSTALLATION_REMOVED;
}

bool IsExtensionInstalled(content::BrowserContext* context,
                          const std::string& extension_id) {
  auto* registry = ExtensionRegistry::Get(context);
  // May be nullptr in unit tests.
  return registry && registry->GetInstalledExtension(extension_id);
}

bool IsExternalExtensionUninstalled(content::BrowserContext* context,
                                    const std::string& extension_id) {
  auto* prefs = ExtensionPrefs::Get(context);
  // May be nullptr in unit tests.
  return prefs && prefs->IsExternalExtensionUninstalled(extension_id);
}

}  // namespace extensions
