// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"

#include "base/stl_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

// The PinnedLauncherApps policy allows specifying three types of identifiers:
// Chrome App Ids, Android App package names, and Web App install URLs. This
// method returns the value that would have been used in the policy to pin an
// app with |app_id|.
//
// Web App Example:
// Admin installs a Web App using "https://foo.example" as the install URL.
// Chrome generates an app id based on the URL e.g. "abc123". Calling
// GetPolicyValueFromAppId() with "abc123" will return "https://foo.example",
// which is the value that would be specified in the PinnedLauncherApps policy
// to pin this Web App.
//
// Arc++ Example:
// Admin installs an Android App with package name "com.example.foo". Chrome
// generates an app id based on the package e.g. "123abc". Calling
// GetPolicyValueFromAppId() with "123abc" will return "com.example.foo", which
// is the value that would be specified in the PinnedLauncherApps policy to
// pin this Android App.
//
// Chrome App Example:
// Admin installs a Chrome App with "aaa111" as its app id. Calling
// GetPolicyValueFromAppId() with "aaa111" will return "aaa111", which is the
// value that would be specified in the PinnedLauncherApps policy to pin this
// Chrome App.
std::string GetPolicyValueFromAppId(const std::string& app_id,
                                    Profile* profile) {
  // Handle Web App ids
  //
  // WebAppProvider is absent in some cases e.g. Arc++ Kiosk Mode.
  if (auto* provider = web_app::WebAppProvider::Get(profile)) {
    std::map<web_app::AppId, GURL> installed_apps =
        provider->registrar().GetExternallyInstalledApps(
            web_app::ExternalInstallSource::kExternalPolicy);
    auto it = installed_apps.find(app_id);
    if (it != installed_apps.end())
      return it->second.spec();
  }

  // Handle Arc++ ids
  const ArcAppListPrefs* const arc_prefs = ArcAppListPrefs::Get(profile);
  if (arc_prefs) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    if (app_info)
      return app_info->package_name;
  }

  // Handle Chrome App ids
  return app_id;
}

}  // namespace

const extensions::Extension* GetExtensionForAppID(const std::string& app_id,
                                                  Profile* profile) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
}

AppListControllerDelegate::Pinnable GetPinnableForAppID(
    const std::string& app_id,
    Profile* profile) {
  // These file manager apps have a shelf presence, but can only be launched
  // when provided a filename to open. Likewise, the feedback extension needs
  // context when launching. Pinning these creates an item that does nothing.
  const char* kNoPinAppIds[] = {
      file_manager::kVideoPlayerAppId,
      file_manager::kGalleryAppId,
      file_manager::kAudioPlayerAppId,
      extension_misc::kFeedbackExtensionId,
  };
  if (base::Contains(kNoPinAppIds, app_id))
    return AppListControllerDelegate::NO_PIN;

  const std::string policy_value_for_id =
      GetPolicyValueFromAppId(app_id, profile);

  if (chromeos::DemoSession::Get() &&
      chromeos::DemoSession::Get()->ShouldIgnorePinPolicy(
          policy_value_for_id)) {
    return AppListControllerDelegate::PIN_EDITABLE;
  }

  const base::ListValue* policy_apps =
      profile->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);
  if (!policy_apps)
    return AppListControllerDelegate::PIN_EDITABLE;

  for (const base::Value& policy_dict_entry : *policy_apps) {
    if (!policy_dict_entry.is_dict())
      return AppListControllerDelegate::PIN_EDITABLE;

    const std::string* policy_entry =
        policy_dict_entry.FindStringKey(kPinnedAppsPrefAppIDKey);
    if (!policy_entry)
      return AppListControllerDelegate::PIN_EDITABLE;

    if (policy_value_for_id == *policy_entry)
      return AppListControllerDelegate::PIN_FIXED;
  }

  return AppListControllerDelegate::PIN_EDITABLE;
}

bool IsCameraApp(const std::string& app_id) {
  return app_id == arc::kCameraAppId || app_id == arc::kLegacyCameraAppId ||
         app_id == arc::kCameraMigrationAppId ||
         app_id == extension_misc::kChromeCameraAppId;
}
