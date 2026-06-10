// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extension_status_utils.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/one_shot_event.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/preinstalled_extensions.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/managed_installation_mode.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

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
  ManagedInstallationMode mode =
      extension ? management->GetInstallationMode(extension)
                : management->GetInstallationMode(extension_id,
                                                  /*update_url=*/std::string());
  return mode == ManagedInstallationMode::kBlocked ||
         mode == ManagedInstallationMode::kRemoved;
}

bool IsExtensionInstalled(content::BrowserContext* context,
                          const std::string& extension_id) {
  auto* registry = ExtensionRegistry::Get(context);
  // May be nullptr in unit tests.
  return registry && registry->GetInstalledExtension(extension_id);
}

bool IsExtensionForceInstalled(content::BrowserContext* context,
                               const std::string& extension_id,
                               std::u16string* reason) {
  auto* registry = ExtensionRegistry::Get(context);
  // May be nullptr in unit tests.
  if (!registry)
    return false;

  auto* extension_system = ExtensionSystem::Get(context);
  if (!extension_system)
    return false;

  const Extension* extension = registry->GetInstalledExtension(extension_id);
  return extension &&
         extension_system->management_policy()->MustRemainInstalled(extension,
                                                                    reason);
}

bool IsExtensionDefaultInstalled(content::BrowserContext* context,
                                 const std::string& extension_id) {
  auto* registry = ExtensionRegistry::Get(context);
  // May be nullptr in unit tests.
  if (!registry)
    return false;
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  return extension &&
         (extension->creation_flags() & Extension::WAS_INSTALLED_BY_DEFAULT);
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

// This is named "apps" for historical reasons.
bool DidPreinstalledAppsPerformNewInstallation(Profile* profile) {
#if !BUILDFLAG(IS_CHROMEOS)
  return preinstalled_extensions::Provider::DidPerformNewInstallationForProfile(
      profile);
#else
  return false;
#endif
}

bool IsPreinstalledAppId(const std::string& app_id) {
  return chrome_app_deprecation::IsPreinstalledAppId(app_id);
}

void SetPreinstalledAppIdForTesting(const char* app_id) {
  chrome_app_deprecation::SetPreinstalledAppIdForTesting(app_id);
}

}  // namespace extensions
