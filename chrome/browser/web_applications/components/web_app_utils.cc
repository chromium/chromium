// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // OS_CHROMEOS

namespace web_app {

constexpr base::FilePath::CharType kWebAppsDirectoryName[] =
    FILE_PATH_LITERAL("WebApps");

bool AreWebAppsEnabled(Profile* profile) {
  if (!profile)
    return false;

  Profile* original_profile = profile->GetOriginalProfile();
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

base::FilePath GetWebAppsDirectory(Profile* profile) {
  return profile->GetPath().Append(base::FilePath(kWebAppsDirectoryName));
}

}  // namespace web_app
