// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_utils.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  if (base::FeatureList::IsEnabled(features::kWebAppsCrosapi))
    return false;
#endif
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsEphemeralGuestProfile() && !profile->IsOffTheRecord();
}

content::BrowserContext* GetBrowserContextForWebApps(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* original_profile =
      Profile::FromBrowserContext(context)->GetOriginalProfile();
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
      !original_profile->IsGuestSession() &&
      !original_profile->IsEphemeralGuestProfile();
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

bool IsChromeOs() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

bool AreFileHandlersAlreadyRegistered(
    Profile* profile,
    const GURL& url,
    const std::vector<blink::Manifest::FileHandler>& new_handlers) {
  if (new_handlers.empty())
    return true;

  const apps::FileHandlers old_handlers =
      GetFileHandlersForAllWebAppsWithOrigin(profile, url);
  const std::set<std::string> mime_types_set =
      apps::GetMimeTypesFromFileHandlers(old_handlers);
  const std::set<std::string> extensions_set =
      apps::GetFileExtensionsFromFileHandlers(old_handlers);

  for (const blink::Manifest::FileHandler& new_handler : new_handlers) {
    for (const auto& new_handler_accept : new_handler.accept) {
      if (!base::Contains(mime_types_set,
                          base::UTF16ToUTF8(new_handler_accept.first))) {
        return false;
      }

      for (const auto& new_extension : new_handler_accept.second) {
        if (!base::Contains(extensions_set, base::UTF16ToUTF8(new_extension)))
          return false;
      }
    }
  }

  return true;
}

apps::FileHandlers GetFileHandlersForAllWebAppsWithOrigin(Profile* profile,
                                                          const GURL& url) {
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  if (!provider)
    return {};

  const AppRegistrar& registrar = provider->registrar();
  std::vector<AppId> app_ids = registrar.FindAppsInScope(url.GetOrigin());
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
    const GURL& url) {
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

  return base::UTF8ToUTF16(base::JoinString(
      associations, l10n_util::GetStringUTF8(
                        IDS_WEB_APP_FILE_HANDLING_EXTENSION_LIST_SEPARATOR)));
}

}  // namespace web_app
