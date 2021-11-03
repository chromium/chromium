// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool g_enable_system_web_apps_in_lacros_for_testing = false;
#endif
}  // namespace

namespace web_app {

constexpr base::FilePath::CharType kManifestResourcesDirectoryName[] =
    FILE_PATH_LITERAL("Manifest Resources");

constexpr base::FilePath::CharType kTempDirectoryName[] =
    FILE_PATH_LITERAL("Temp");

bool AreWebAppsEnabled(const Profile* profile) {
  if (!profile || profile->IsSystemProfile())
    return false;

  const Profile* original_profile = profile->GetOriginalProfile();
  DCHECK(!original_profile->IsOffTheRecord());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Web Apps should not be installed to the ChromeOS system profiles.
  if (!chromeos::ProfileHelper::IsRegularProfile(original_profile)) {
    return false;
  }
  // Disable Web Apps if running any kiosk app.
  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager && user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return true;
}

bool AreWebAppsUserInstallable(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // With Lacros, web apps are not installed using the Ash browser.
  if (IsWebAppsCrosapiEnabled())
    return false;
#endif
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
}

bool AreSystemWebAppsSupported() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!g_enable_system_web_apps_in_lacros_for_testing)
    return false;
#endif
  return true;
}

content::BrowserContext* GetBrowserContextForWebApps(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }
  Profile* original_profile = profile->GetOriginalProfile();
  return AreWebAppsEnabled(original_profile) ? original_profile : nullptr;
}

content::BrowserContext* GetBrowserContextForWebAppMetrics(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* original_profile =
      Profile::FromBrowserContext(context)->GetOriginalProfile();
  const bool is_web_app_metrics_enabled =
      site_engagement::SiteEngagementService::IsEnabled() &&
      AreWebAppsEnabled(original_profile) &&
      !original_profile->IsGuestSession();
  return is_web_app_metrics_enabled ? original_profile : nullptr;
}

base::FilePath GetWebAppsRootDirectory(Profile* profile) {
  return profile->GetPath().Append(chrome::kWebAppDirname);
}

base::FilePath GetManifestResourcesDirectory(
    const base::FilePath& web_apps_root_directory) {
  return web_apps_root_directory.Append(kManifestResourcesDirectoryName);
}

base::FilePath GetManifestResourcesDirectory(Profile* profile) {
  return GetManifestResourcesDirectory(GetWebAppsRootDirectory(profile));
}

base::FilePath GetManifestResourcesDirectoryForApp(
    const base::FilePath& web_apps_root_directory,
    const AppId& app_id) {
  return GetManifestResourcesDirectory(web_apps_root_directory)
      .AppendASCII(app_id);
}

base::FilePath GetWebAppsTempDirectory(
    const base::FilePath& web_apps_root_directory) {
  return web_apps_root_directory.Append(kTempDirectoryName);
}

std::string GetProfileCategoryForLogging(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!chromeos::ProfileHelper::IsRegularProfile(profile)) {
    return "SigninOrLockScreen";
  } else if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return "Kiosk";
  } else if (chromeos::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return "Ephemeral";
  } else if (chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return "Primary";
  } else {
    return "Other";
  }
#else
  // Chrome OS profiles are different from non-ChromeOS ones. Because System Web
  // Apps are not installed on non Chrome OS, "Other" is returned here.
  return "Other";
#endif
}

bool IsChromeOsDataMandatory() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool AreAppsLocallyInstalledBySync() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Chrome OS, sync always locally installs an app.
  return true;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // With Crosapi, Ash no longer participates in sync.
  // On Chrome OS before Crosapi, sync always locally installs an app.
  return !IsWebAppsCrosapiEnabled();
#else
  return false;
#endif
}

bool AreNewFileHandlersASubsetOfOld(const apps::FileHandlers& old_handlers,
                                    const apps::FileHandlers& new_handlers) {
  if (new_handlers.empty())
    return true;

  const std::set<std::string> mime_types_set =
      apps::GetMimeTypesFromFileHandlers(old_handlers);
  const std::set<std::string> extensions_set =
      apps::GetFileExtensionsFromFileHandlers(old_handlers);

  for (const apps::FileHandler& new_handler : new_handlers) {
    for (const auto& new_handler_accept : new_handler.accept) {
      if (!base::Contains(mime_types_set, new_handler_accept.mime_type)) {
        return false;
      }

      for (const auto& new_extension : new_handler_accept.file_extensions) {
        if (!base::Contains(extensions_set, new_extension))
          return false;
      }
    }
  }

  return true;
}

bool AreFileHandlersAlreadyRegistered(Profile* profile,
                                      const GURL& url,
                                      const apps::FileHandlers& new_handlers) {
  return AreNewFileHandlersASubsetOfOld(
      GetFileHandlersForAllWebAppsWithOrigin(profile, url), new_handlers);
}

apps::FileHandlers GetFileHandlersForAllWebAppsWithOrigin(Profile* profile,
                                                          const GURL& url) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider)
    return {};

  const WebAppRegistrar& registrar = provider->registrar();
  std::vector<AppId> app_ids =
      registrar.FindAppsInScope(url.DeprecatedGetOriginAsURL());
  if (app_ids.empty())
    return {};

  apps::FileHandlers aggregated_handlers;
  for (const AppId& app_id : app_ids) {
    const apps::FileHandlers* handlers = registrar.GetAppFileHandlers(app_id);
    aggregated_handlers.insert(aggregated_handlers.end(), handlers->begin(),
                               handlers->end());
  }

  return aggregated_handlers;
}

std::u16string GetFileTypeAssociationsHandledByWebAppsForDisplay(
    Profile* profile,
    const GURL& url,
    bool* found_multiple) {
  const apps::FileHandlers file_handlers =
      GetFileHandlersForAllWebAppsWithOrigin(profile, url);
  std::vector<std::string> associations;
#if defined(OS_LINUX)
  std::set<std::string> mime_types_set =
      apps::GetMimeTypesFromFileHandlers(file_handlers);
  associations.reserve(mime_types_set.size());
  associations.insert(associations.end(), mime_types_set.begin(),
                      mime_types_set.end());
#else   // !defined(OS_LINUX)
  std::set<std::string> extensions_set =
      apps::GetFileExtensionsFromFileHandlers(file_handlers);
  associations.reserve(extensions_set.size());

  // Convert file types from formats like ".txt" to "TXT".
  std::transform(extensions_set.begin(), extensions_set.end(),
                 std::back_inserter(associations),
                 [](const std::string& extension) {
                   return base::ToUpperASCII(extension.substr(1));
                 });
#endif  // defined(OS_LINUX)

  if (found_multiple)
    *found_multiple = associations.size() > 1;

  return base::UTF8ToUTF16(base::JoinString(
      associations,
      l10n_util::GetStringUTF8(IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR)));
}

std::u16string GetFileTypeAssociationsHandledByWebAppForDisplay(
    Profile* profile,
    const AppId& app_id,
    bool* found_multiple) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider)
    return {};

  const apps::FileHandlers* file_handlers =
      provider->registrar().GetAppFileHandlers(app_id);

  std::vector<std::string> associations;
#if defined(OS_LINUX)
  // TODO(estade): on Linux both the MIME type and extension must match. Should
  // we just show the extensions like on other platforms?
  std::set<std::string> mime_types_set =
      apps::GetMimeTypesFromFileHandlers(*file_handlers);
  associations.reserve(mime_types_set.size());
  associations.insert(associations.end(), mime_types_set.begin(),
                      mime_types_set.end());
#else   // !defined(OS_LINUX)
  std::set<std::string> extensions_set =
      apps::GetFileExtensionsFromFileHandlers(*file_handlers);
  associations.reserve(extensions_set.size());

  // Convert file types from formats like ".txt" to "TXT".
  std::transform(extensions_set.begin(), extensions_set.end(),
                 std::back_inserter(associations),
                 [](const std::string& extension) {
                   return base::ToUpperASCII(extension.substr(1));
                 });
#endif  // defined(OS_LINUX)

  if (found_multiple)
    *found_multiple = associations.size() > 1;

  return base::UTF8ToUTF16(base::JoinString(
      associations,
      l10n_util::GetStringUTF8(IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsWebAppsCrosapiEnabled() {
  return base::FeatureList::IsEnabled(features::kWebAppsCrosapi) ||
         base::FeatureList::IsEnabled(chromeos::features::kLacrosPrimary);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void EnableSystemWebAppsInLacrosForTesting() {
  g_enable_system_web_apps_in_lacros_for_testing = true;
}
#endif

void PersistProtocolHandlersUserChoice(
    Profile* profile,
    const AppId& app_id,
    const GURL& protocol_url,
    bool allowed,
    base::OnceClosure update_finished_callback) {
  web_app::WebAppProvider* const provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);

  web_app::OsIntegrationManager& os_integration_manager =
      provider->os_integration_manager();
  const std::vector<ProtocolHandler> original_protocol_handlers =
      os_integration_manager.GetAppProtocolHandlers(app_id);

  if (allowed) {
    provider->sync_bridge().AddAllowedLaunchProtocol(app_id,
                                                     protocol_url.scheme());
  } else {
    provider->sync_bridge().AddDisallowedLaunchProtocol(app_id,
                                                        protocol_url.scheme());
  }

  // OS protocol registration does not need to be updated.
  if (original_protocol_handlers ==
      os_integration_manager.GetAppProtocolHandlers(app_id)) {
    std::move(update_finished_callback).Run();
    return;
  }

  // TODO(https://crbug.com/1251062): Can we avoid the delay of startup, if the
  // action as allowed?
  provider->os_integration_manager().UpdateProtocolHandlers(
      app_id, /*force_shortcut_updates_if_needed=*/true,
      std::move(update_finished_callback));
}

void PersistFileHandlersUserChoice(Profile* profile,
                                   const AppId& app_id,
                                   bool allowed,
                                   base::OnceClosure update_finished_callback) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kDesktopPWAsFileHandlingSettingsGated));
  web_app::WebAppProvider* const provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);

  {
    ScopedRegistryUpdate update(&provider->sync_bridge());
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetFileHandlerApprovalState(
        allowed ? ApiApprovalState::kAllowed : ApiApprovalState::kDisallowed);
  }

  if (allowed) {
    std::move(update_finished_callback).Run();
  } else {
    provider->os_integration_manager().UpdateFileHandlers(
        app_id, FileHandlerUpdateAction::kRemove,
        std::move(update_finished_callback));
  }
}

}  // namespace web_app
