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
#include "base/syslog_logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
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
      entry.FindKey(kCreateDesktopShortcutKey);
  const base::Value* fallback_app_name = entry.FindKey(kFallbackAppNameKey);

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

  // Allow administrators to override the name of the placeholder app, as well
  // as the permanent name for Web Apps without a manifest.
  if (fallback_app_name)
    install_options.fallback_app_name = fallback_app_name->GetString();

  return install_options;
}

}  // namespace

const char WebAppPolicyManager::kInstallResultHistogramName[];

WebAppPolicyManager::WebAppPolicyManager(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      default_settings_(
          std::make_unique<WebAppPolicyManager::WebAppSetting>()) {}

WebAppPolicyManager::~WebAppPolicyManager() = default;

void WebAppPolicyManager::SetSubsystems(
    PendingAppManager* pending_app_manager,
    AppRegistrar* app_registrar,
    AppRegistryController* app_registry_controller,
    SystemWebAppManager* web_app_manager,
    OsIntegrationManager* os_integration_manager) {
  DCHECK(pending_app_manager);
  DCHECK(app_registrar);
  DCHECK(app_registry_controller);
  DCHECK(os_integration_manager);

  pending_app_manager_ = pending_app_manager;
  app_registrar_ = app_registrar;
  app_registry_controller_ = app_registry_controller;
  web_app_manager_ = web_app_manager;
  os_integration_manager_ = os_integration_manager;
}

void WebAppPolicyManager::Start() {
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicy,
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
  install_options.run_on_os_login =
      (GetUrlRunOnOsLoginPolicy(install_options.install_url) ==
       RunOnOsLoginPolicy::kRunWindowed);

  // If the app is not a placeholder app, PendingAppManager will ignore the
  // request.
  pending_app_manager_->Install(std::move(install_options), base::DoNothing());
}

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
  registry->RegisterDictionaryPref(prefs::kWebAppSettings);
}

void WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicy() {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kWebAppSettings,
      base::BindRepeating(&WebAppPolicyManager::RefreshPolicySettings,
                          weak_ptr_factory_.GetWeakPtr()));

  RefreshPolicySettings();
  RefreshPolicyInstalledApps();
  ObserveDisabledSystemFeaturesPolicy();
}

void WebAppPolicyManager::OnDisableListPolicyChanged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PopulateDisabledWebAppsIdsLists();
  std::vector<web_app::AppId> app_ids = app_registrar_->GetAppIds();
  for (const auto& id : app_ids) {
    const bool is_disabled = base::Contains(disabled_web_apps_, id);
    app_registry_controller_->SetAppIsDisabled(id, is_disabled);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

const std::set<SystemAppType>& WebAppPolicyManager::GetDisabledSystemWebApps()
    const {
  return disabled_system_apps_;
}

const std::set<AppId>& WebAppPolicyManager::GetDisabledWebAppsIds() const {
  return disabled_web_apps_;
}

bool WebAppPolicyManager::IsDisabledAppsModeHidden() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return false;

  std::string disabled_mode =
      local_state->GetString(policy::policy_prefs::kSystemFeaturesDisableMode);
  if (disabled_mode == policy::kHiddenDisableMode)
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
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
    install_options.run_on_os_login =
        (GetUrlRunOnOsLoginPolicy(install_options.install_url) ==
         RunOnOsLoginPolicy::kRunWindowed);

    install_options_list.push_back(std::move(install_options));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      base::BindOnce(&WebAppPolicyManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppPolicyManager::RefreshPolicySettings() {
  // No need to validate the types or values of the policy members because we
  // are using a SimpleSchemaValidatingPolicyHandler which should validate them
  // for us.
  const base::DictionaryValue* web_app_dict =
      pref_service_->GetDictionary(prefs::kWebAppSettings);

  settings_by_url_.clear();
  default_settings_ = std::make_unique<WebAppPolicyManager::WebAppSetting>();

  if (!web_app_dict)
    return;

  // Read default policy, if provided.
  const base::DictionaryValue* default_settings_dict = nullptr;
  if (web_app_dict->GetDictionary(kWildcard, &default_settings_dict)) {
    if (!default_settings_->Parse(default_settings_dict, true)) {
      SYSLOG(WARNING) << "Malformed default web app management setting.";
      default_settings_->ResetSettings();
    }
  }

  // Read policy for individual web apps
  for (base::DictionaryValue::Iterator iter(*web_app_dict); !iter.IsAtEnd();
       iter.Advance()) {
    if (iter.key() == kWildcard)
      continue;

    const base::DictionaryValue* web_app_settings_dict;
    if (!iter.value().GetAsDictionary(&web_app_settings_dict))
      continue;

    GURL url = GURL(iter.key());
    if (!url.is_valid()) {
      LOG(WARNING) << "Invalid URL: " << iter.key();
      continue;
    }

    WebAppPolicyManager::WebAppSetting by_url(*default_settings_);
    if (by_url.Parse(web_app_settings_dict, false)) {
      settings_by_url_[url] = by_url;
    } else {
      LOG(WARNING) << "Malformed web app settings for " << url;
    }
  }

  ApplyPolicySettings();

  if (refresh_policy_settings_completed_)
    std::move(refresh_policy_settings_completed_).Run();
}

void WebAppPolicyManager::ApplyPolicySettings() {
  std::map<AppId, GURL> policy_installed_apps_ =
      app_registrar_->GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  for (const AppId& app_id : app_registrar_->GetAppIds()) {
    RunOnOsLoginPolicy policy =
        GetUrlRunOnOsLoginPolicy(policy_installed_apps_[app_id]);
    if (policy == RunOnOsLoginPolicy::kBlocked) {
      app_registry_controller_->SetAppRunOnOsLoginMode(
          app_id, RunOnOsLoginMode::kNotRun);
      OsHooksResults os_hooks;
      os_hooks[OsHookType::kRunOnOsLogin] = true;
      os_integration_manager_->UninstallOsHooks(app_id, os_hooks,
                                                base::DoNothing());
    } else if (policy == RunOnOsLoginPolicy::kRunWindowed) {
      app_registry_controller_->SetAppRunOnOsLoginMode(
          app_id, RunOnOsLoginMode::kWindowed);
      InstallOsHooksOptions options;
      options.os_hooks[OsHookType::kRunOnOsLogin] = true;
      os_integration_manager_->InstallOsHooks(app_id, base::DoNothing(),
                                              nullptr, options);
    }
  }

  for (WebAppPolicyManagerObserver& observer : observers_)
    observer.OnPolicyChanged();
}

void WebAppPolicyManager::AddObserver(WebAppPolicyManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppPolicyManager::RemoveObserver(
    WebAppPolicyManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

RunOnOsLoginPolicy WebAppPolicyManager::GetUrlRunOnOsLoginPolicy(
    base::Optional<GURL> url) const {
  if (url) {
    auto it = settings_by_url_.find(url.value());
    if (it != settings_by_url_.end())
      return it->second.run_on_os_login_policy;
  }
  return default_settings_->run_on_os_login_policy;
}

void WebAppPolicyManager::SetOnAppsSynchronizedCompletedCallbackForTesting(
    base::OnceClosure callback) {
  on_apps_synchronized_ = std::move(callback);
}

void WebAppPolicyManager::SetRefreshPolicySettingsCompletedCallbackForTesting(
    base::OnceClosure callback) {
  refresh_policy_settings_completed_ = std::move(callback);
}

void WebAppPolicyManager::OnAppsSynchronized(
    std::map<GURL, PendingAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  is_refreshing_ = false;

  if (!install_results.empty())
    ApplyPolicySettings();

  if (needs_refresh_)
    RefreshPolicyInstalledApps();

  for (const auto& url_and_result : install_results) {
    base::UmaHistogramEnumeration(kInstallResultHistogramName,
                                  url_and_result.second.code);
  }

  if (on_apps_synchronized_)
    std::move(on_apps_synchronized_).Run();
}

WebAppPolicyManager::WebAppSetting::WebAppSetting() {
  ResetSettings();
}

bool WebAppPolicyManager::WebAppSetting::Parse(
    const base::DictionaryValue* dict,
    bool for_default_settings) {
  std::string run_on_os_login_str;
  if (dict->GetStringWithoutPathExpansion(kRunOnOsLogin,
                                          &run_on_os_login_str)) {
    if (run_on_os_login_str == kAllowed) {
      run_on_os_login_policy = RunOnOsLoginPolicy::kAllowed;
    } else if (run_on_os_login_str == kBlocked) {
      run_on_os_login_policy = RunOnOsLoginPolicy::kBlocked;
    } else if (!for_default_settings && run_on_os_login_str == kRunWindowed) {
      run_on_os_login_policy = RunOnOsLoginPolicy::kRunWindowed;
    } else {
      SYSLOG(WARNING) << "Malformed web app run on os login preference.";
      return false;
    }
  }

  return true;
}

void WebAppPolicyManager::WebAppSetting::ResetSettings() {
  run_on_os_login_policy = RunOnOsLoginPolicy::kAllowed;
}

void WebAppPolicyManager::ObserveDisabledSystemFeaturesPolicy() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state) {  // Sometimes it's not available in tests.
    return;
  }
  local_state_pref_change_registrar_.Init(local_state);

  local_state_pref_change_registrar_.Add(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::BindRepeating(&WebAppPolicyManager::OnDisableListPolicyChanged,
                          base::Unretained(this)));
  local_state_pref_change_registrar_.Add(
      policy::policy_prefs::kSystemFeaturesDisableMode,
      base::BindRepeating(&WebAppPolicyManager::OnDisableModePolicyChanged,
                          base::Unretained(this)));
  // Make sure we get the right disabled mode in case it was changed before
  // policy registration.
  OnDisableModePolicyChanged();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void WebAppPolicyManager::OnDisableModePolicyChanged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_registry_controller_->UpdateAppsDisableMode();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void WebAppPolicyManager::PopulateDisabledWebAppsIdsLists() {
  disabled_system_apps_.clear();
  disabled_web_apps_.clear();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return;

  const base::ListValue* disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  if (!disabled_system_features_pref)
    return;

  for (const auto& entry : *disabled_system_features_pref) {
    switch (entry.GetInt()) {
      case policy::SystemFeature::kCamera:
        disabled_system_apps_.insert(SystemAppType::CAMERA);
        break;
      case policy::SystemFeature::kOsSettings:
        disabled_system_apps_.insert(SystemAppType::SETTINGS);
        break;
      case policy::SystemFeature::kScanning:
        disabled_system_apps_.insert(SystemAppType::SCANNING);
        break;
      case policy::SystemFeature::kCanvas:
        disabled_web_apps_.insert(web_app::kCanvasAppId);
        break;
      case policy::SystemFeature::kGoogleNews:
        disabled_web_apps_.insert(web_app::kGoogleNewsAppId);
        break;
      case policy::SystemFeature::kExplore:
        disabled_web_apps_.insert(web_app::kHelpAppId);
        break;
    }
  }

  for (const auto& app_type : disabled_system_apps_) {
    base::Optional<AppId> app_id =
        web_app_manager_->GetAppIdForSystemApp(app_type);
    if (app_id.has_value()) {
      disabled_web_apps_.insert(app_id.value());
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace web_app
