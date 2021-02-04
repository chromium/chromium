// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

ExternalInstallOptions ParseInstallOptionsFromPolicyEntry(
    const base::Value& entry) {
  const base::Value& url = *entry.FindKey(kUrlKey);
  const base::Value* default_launch_container =
      entry.FindKey(kDefaultLaunchContainerKey);
  const base::Value* create_desktop_shortcut =
      entry.FindKey(kCreateDesktopShorcutKey);

  DCHECK(!default_launch_container ||
         default_launch_container->GetString() ==
             kDefaultLaunchContainerWindowValue ||
         default_launch_container->GetString() ==
             kDefaultLaunchContainerTabValue);

  DisplayMode user_display_mode;
  if (!default_launch_container) {
    user_display_mode = DisplayMode::kBrowser;
  } else if (default_launch_container->GetString() ==
             kDefaultLaunchContainerTabValue) {
    user_display_mode = DisplayMode::kBrowser;
  } else {
    user_display_mode = DisplayMode::kStandalone;
  }

  ExternalInstallOptions install_options{
      GURL(url.GetString()), user_display_mode,
      ExternalInstallSource::kExternalPolicy};

  install_options.add_to_applications_menu = true;
  install_options.add_to_desktop =
      create_desktop_shortcut ? create_desktop_shortcut->GetBool() : false;
  // Pinning apps to the ChromeOS shelf is done through the PinnedLauncherApps
  // policy.
  install_options.add_to_quick_launch_bar = false;

  return install_options;
}

}  // namespace

const char WebAppPolicyManager::kInstallResultHistogramName[];

WebAppPolicyManager::WebAppPolicyManager(Profile* profile)
    : profile_(profile), pref_service_(profile_->GetPrefs()) {}

WebAppPolicyManager::~WebAppPolicyManager() = default;

void WebAppPolicyManager::SetSubsystems(
    PendingAppManager* pending_app_manager,
    AppRegistrar* app_registrar,
    AppRegistryController* app_registry_controller,
    SystemWebAppManager* web_app_manager) {
  pending_app_manager_ = pending_app_manager;
  app_registrar_ = app_registrar;
  app_registry_controller_ = app_registry_controller;
  web_app_manager_ = web_app_manager;
}

void WebAppPolicyManager::Start() {
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&WebAppPolicyManager::
                             InitChangeRegistrarAndRefreshPolicyInstalledApps,
                         weak_ptr_factory_.GetWeakPtr()));
}

void WebAppPolicyManager::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  const auto& web_apps_list = web_apps->GetList();

  const auto it =
      std::find_if(web_apps_list.begin(), web_apps_list.end(),
                   [&url](const base::Value& entry) {
                     return entry.FindKey(kUrlKey)->GetString() == url.spec();
                   });

  if (it == web_apps_list.end())
    return;

  ExternalInstallOptions install_options =
      ParseInstallOptionsFromPolicyEntry(*it);

  // No need to install a placeholder because there should be one already.
  install_options.wait_for_windows_closed = true;
  install_options.reinstall_placeholder = true;

  // If the app is not a placeholder app, PendingAppManager will ignore the
  // request.
  pending_app_manager_->Install(std::move(install_options), base::DoNothing());
}

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
}

void WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicyInstalledApps() {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                          weak_ptr_factory_.GetWeakPtr()));

  RefreshPolicyInstalledApps();
  ObserveSystemDisableListPolicy();
}

void WebAppPolicyManager::OnAppsPolicyChanged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto disabled_web_apps = GetDisabledWebAppsIds();
  std::vector<web_app::AppId> app_ids = app_registrar_->GetAppIds();
  for (const auto& id : app_ids) {
    const bool is_disabled = base::Contains(disabled_web_apps, id);
    app_registry_controller_->SetAppIsDisabled(id, is_disabled);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

std::set<SystemAppType> WebAppPolicyManager::GetDisabledSystemWebApps() const {
  std::set<SystemAppType> disabled_system_apps;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return disabled_system_apps;

  const base::ListValue* disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  if (!disabled_system_features_pref)
    return disabled_system_apps;

  for (const auto& entry : *disabled_system_features_pref) {
    switch (entry.GetInt()) {
      case policy::SystemFeature::kCamera:
        disabled_system_apps.insert(SystemAppType::CAMERA);
        break;
      case policy::SystemFeature::kOsSettings:
        disabled_system_apps.insert(SystemAppType::SETTINGS);
        break;
      case policy::SystemFeature::kScanning:
        disabled_system_apps.insert(SystemAppType::SCANNING);
        break;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return disabled_system_apps;
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  // If this is called again while in progress, we will run it again once the
  // |SynchronizeInstalledApps| call is finished.
  if (is_refreshing_) {
    needs_refresh_ = true;
    return;
  }

  is_refreshing_ = true;
  needs_refresh_ = false;

  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  std::vector<ExternalInstallOptions> install_options_list;
  // No need to validate the types or values of the policy members because we
  // are using a SimpleSchemaValidatingPolicyHandler which should validate them
  // for us.
  for (const base::Value& entry : web_apps->GetList()) {
    ExternalInstallOptions install_options =
        ParseInstallOptionsFromPolicyEntry(entry);

    install_options.install_placeholder = true;
    // When the policy gets refreshed, we should try to reinstall placeholder
    // apps but only if they are not being used.
    install_options.wait_for_windows_closed = true;
    install_options.reinstall_placeholder = true;

    install_options_list.push_back(std::move(install_options));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      base::BindOnce(&WebAppPolicyManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppPolicyManager::OnAppsSynchronized(
    std::map<GURL, PendingAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  is_refreshing_ = false;
  if (needs_refresh_)
    RefreshPolicyInstalledApps();

  for (const auto& url_and_result : install_results) {
    base::UmaHistogramEnumeration(kInstallResultHistogramName,
                                  url_and_result.second.code);
  }
}

void WebAppPolicyManager::ObserveSystemDisableListPolicy() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state) {  // Sometimes it's not available in tests.
    return;
  }
  local_state_pref_change_registrar_.Init(local_state);

  local_state_pref_change_registrar_.Add(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::BindRepeating(&WebAppPolicyManager::OnAppsPolicyChanged,
                          base::Unretained(this)));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

std::set<AppId> WebAppPolicyManager::GetDisabledWebAppsIds() const {
  std::set<AppId> disabled_web_apps;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto disabled_system_apps = GetDisabledSystemWebApps();
  for (const auto& app_type : disabled_system_apps) {
    base::Optional<AppId> app_id =
        web_app_manager_->GetAppIdForSystemApp(app_type);
    if (app_id.has_value()) {
      disabled_web_apps.insert(app_id.value());
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return disabled_web_apps;
}

}  // namespace web_app
