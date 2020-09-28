// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // OS_CHROMEOS

namespace web_app {

constexpr base::FilePath::CharType kManifestResourcesDirectoryName[] =
    FILE_PATH_LITERAL("Manifest Resources");

constexpr base::FilePath::CharType kTempDirectoryName[] =
    FILE_PATH_LITERAL("Temp");

bool AreWebAppsEnabled(const Profile* profile) {
  if (!profile)
    return false;

  const Profile* original_profile = profile->GetOriginalProfile();
  DCHECK(!original_profile->IsOffTheRecord());

  if (original_profile->IsSystemProfile())
    return false;

#if defined(OS_CHROMEOS)
  // Web Apps should not be installed to the ChromeOS system profiles.
  if (chromeos::ProfileHelper::IsSigninProfile(original_profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(original_profile)) {
    return false;
  }
  // Disable Web Apps if running any kiosk app.
  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager && (user_manager->IsLoggedInAsKioskApp() ||
                       user_manager->IsLoggedInAsArcKioskApp())) {
    return false;
  }
#endif  // OS_CHROMEOS

  return true;
}

bool AreWebAppsUserInstallable(Profile* profile) {
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
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
  const bool is_web_app_metrics_enabled = AreWebAppsEnabled(original_profile) &&
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
#ifdef OS_CHROMEOS
  if (chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
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
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

}  // namespace web_app
