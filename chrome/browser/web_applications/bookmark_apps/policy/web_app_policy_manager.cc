// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // OS_CHROMEOS

namespace web_app {

WebAppPolicyManager::WebAppPolicyManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      pending_app_manager_(pending_app_manager) {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      base::BindOnce(&WebAppPolicyManager::
                         InitChangeRegistrarAndRefreshPolicyInstalledApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

WebAppPolicyManager::~WebAppPolicyManager() = default;

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
}

// static
bool WebAppPolicyManager::ShouldEnableForProfile(Profile* profile) {
// PolicyBrowserTests applies test policies to all profiles, including the
// sign-in profile. This causes tests to become flaky since the tests could
// finish before, during, or after the policy apps fail to install in the
// sign-in profile. So we temporarily add a guard to ignore the policy for the
// sign-in profile.
// TODO(crbug.com/876705): Remove once the policy no longer applies to the
// sign-in profile during tests.
#if defined(OS_CHROMEOS)
  return !chromeos::ProfileHelper::IsSigninProfile(profile);
#else  // !OS_CHROMEOS
  return true;
#endif
}

void WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicyInstalledApps() {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                          weak_ptr_factory_.GetWeakPtr()));

  RefreshPolicyInstalledApps();
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  std::vector<PendingAppManager::AppInfo> apps_to_install;
  for (const base::Value& info : web_apps->GetList()) {
    const base::Value& url = *info.FindKey(kUrlKey);
    const base::Value* launch_container = info.FindKey(kLaunchContainerKey);

    DCHECK(!launch_container ||
           launch_container->GetString() == kLaunchContainerWindowValue ||
           launch_container->GetString() == kLaunchContainerTabValue);

    LaunchContainer container;
    if (!launch_container)
      container = LaunchContainer::kDefault;
    else if (launch_container->GetString() == kLaunchContainerWindowValue)
      container = LaunchContainer::kWindow;
    else
      container = LaunchContainer::kTab;

    // There is a separate policy to create shortcuts/pin apps to shelf.
    apps_to_install.emplace_back(GURL(url.GetString()), container,
                                 web_app::InstallSource::kExternalPolicy,
                                 false /* create_shortcuts */);
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(apps_to_install), InstallSource::kExternalPolicy);
}

}  // namespace web_app
