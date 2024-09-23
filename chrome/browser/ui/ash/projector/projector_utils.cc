// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/untrusted_projector_ui.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_launch_params.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace {

bool IsRealUserProfile(const Profile* profile) {
  // Return false for signin, lock screen and incognito profiles.
  return ash::ProfileHelper::IsUserProfile(profile) &&
         !profile->IsOffTheRecord();
}

}  // namespace

bool IsProjectorAllowedForProfile(const Profile* profile) {
  DCHECK(profile);
  if (!IsRealUserProfile(profile))
    return false;

  auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;

  return user->HasGaiaAccount();
}

bool IsProjectorAppEnabled(const Profile* profile) {
  if (!IsProjectorAllowedForProfile(profile))
    return false;

  // Projector for regular consumer users.
  if (!profile->GetProfilePolicyConnector()->IsManaged())
    return true;

  // Projector dogfood for supervised users is controlled by an enterprise
  // policy. When the feature is out of dogfood phase the policy will be
  // deprecated and the feature will be enabled by default.
  if (profile->IsChild()) {
    return profile->GetPrefs()->GetBoolean(
        ash::prefs::kProjectorDogfoodForFamilyLinkEnabled);
  }

  // Projector for enterprise users is controlled by a combination of a feature
  // flag and an enterprise policy.
  return ash::features::IsProjectorManagedUserIgnorePolicyEnabled() ||
         profile->GetPrefs()->GetBoolean(ash::prefs::kProjectorAllowByPolicy);
}

bool IsMediaFile(const base::FilePath& path) {
  return path.MatchesExtension(ash::kProjectorMediaFileExtension);
}

bool IsMetadataFile(const base::FilePath& path) {
  return path.MatchesExtension(ash::kProjectorMetadataFileExtension) ||
         path.MatchesExtension(ash::kProjectorV2MetadataFileExtension);
}

void SendFilesToProjectorApp(std::vector<base::FilePath> files) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::PROJECTOR);
  if (!browser) {
    // Do not call SendFilesToProjectorApp() unless the Projector app is already
    // open.
    return;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* web_ui = web_contents->GetWebUI();
  if (!web_ui)
    return;
  if (!web_ui->GetController()->GetAs<ash::UntrustedProjectorUI>()) {
    // We only want to send files to the Projector SWA. Don't send files to the
    // wrong trusted context if it navigates away.
    // TODO(b/237089852): Consider using a navigation throttle to prevent the
    // trusted contents from navigating away, but this check is still useful as
    // an extra precaution.
    return;
  }

  web_app::WebAppLaunchParams launch_params;
  launch_params.started_new_navigation = false;
  launch_params.app_id = ash::kChromeUIUntrustedProjectorSwaAppId;
  // Sending files should not navigate the app. This argument is used for
  // storage isolation, and won't impact navigation. It should be in scope of
  // the current WebContent's origin.
  launch_params.target_url = web_contents->GetVisibleURL();
  launch_params.paths = std::move(files);
  web_app::WebAppTabHelper::FromWebContents(web_contents)
      ->EnsureLaunchQueue()
      .Enqueue(std::move(launch_params));
}
