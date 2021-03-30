// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extension_status_utils.h"

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"

namespace {

const char* g_default_app_for_testing = nullptr;

}  // namespace

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

void OnExtensionSystemReady(content::BrowserContext* context,
                            base::OnceClosure callback) {
  ExtensionSystem::Get(context)->ready().Post(FROM_HERE, std::move(callback));
}

bool DidDefaultAppsPerformNewInstallation(Profile* profile) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  return default_apps::Provider::DidPerformNewInstallationForProfile(profile);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool IsDefaultAppId(const std::string& app_id) {
  if (g_default_app_for_testing && app_id == g_default_app_for_testing)
    return true;

  return app_id == extension_misc::kGmailAppId ||
         app_id == extension_misc::kGoogleDocAppId ||
         app_id == extension_misc::kDriveHostedAppId ||
         app_id == extension_misc::kGoogleSheetsAppId ||
         app_id == extension_misc::kGoogleSlidesAppId ||
         app_id == extension_misc::kYoutubeAppId;
}

void SetDefaultAppIdForTesting(const char* app_id) {
  g_default_app_for_testing = app_id;
}

}  // namespace extensions
