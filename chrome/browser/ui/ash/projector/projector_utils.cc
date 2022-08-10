// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/prefs/pref_service.h"

namespace {

bool IsRealUserProfile(const Profile* profile) {
  // Return false for signin, lock screen and incognito profiles.
  return chromeos::ProfileHelper::IsRegularProfile(profile) &&
         !profile->IsOffTheRecord();
}

}  // namespace

bool IsProjectorAllowedForProfile(const Profile* profile) {
  DCHECK(profile);
  if (!IsRealUserProfile(profile))
    return false;

  auto* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;

  return user->HasGaiaAccount();
}

bool IsProjectorAppEnabled(const Profile* profile) {
  if (!IsProjectorAllowedForProfile(profile))
    return false;

  // Projector for regular consumer users is controlled by a feature flag.
  if (!profile->GetProfilePolicyConnector()->IsManaged())
    return ash::features::IsProjectorAllUserEnabled();

  // Projector dogfood for supervised users is controlled by an enterprise
  // policy. When the feature is out of dogfood phase the policy will be
  // deprecated and the feature will be enabled by default.
  if (profile->IsChild()) {
    return profile->GetPrefs()->GetBoolean(
        ash::prefs::kProjectorDogfoodForFamilyLinkEnabled);
  }

  // Projector for enterprise users is controlled by a combination of a feature
  // flag and an enterprise policy.
  return ash::features::IsProjectorEnabled() &&
         (ash::features::IsProjectorManagedUserIgnorePolicyEnabled() ||
          profile->GetPrefs()->GetBoolean(ash::prefs::kProjectorAllowByPolicy));
}

void LaunchProjectorAppWithFiles(std::vector<base::FilePath> files) {
  auto* profile = ProfileManager::GetActiveUserProfile();
  ash::SystemAppLaunchParams params;
  params.launch_paths = std::move(files);
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::PROJECTOR,
                               params);
}
