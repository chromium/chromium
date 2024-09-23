// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

BASE_DECLARE_FEATURE(kDesktopPWAsForceUnregisterOSIntegration);

class WebAppProvider;

// Policy installation allows enterprise admins to control and manage
// Web Apps on behalf of their managed users. This class tracks the policy that
// affects Web Apps and also tracks which Web Apps are currently installed based
// on this policy. Based on these, it decides which apps to install, uninstall,
// and update, via a ExternallyManagedAppManager.
class WebAppPolicyManager {
 public:
  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.Policy";

  // Constructs a WebAppPolicyManager instance that uses
  // |externally_managed_app_manager| to manage apps.
  // |externally_managed_app_manager| should outlive this class.
  explicit WebAppPolicyManager(Profile* profile);
  WebAppPolicyManager(const WebAppPolicyManager&) = delete;
  WebAppPolicyManager& operator=(const WebAppPolicyManager&) = delete;
  ~WebAppPolicyManager();

  void set_provider(base::PassKey<WebAppProvider>, WebAppProvider& provider) {
    provider_ = &provider;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSystemWebAppDelegateMap(
      const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map);
#endif

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  // `policy_settings_and_force_installs_applied_` waits for the first
  // `SynchronizeInstalledApps` to finish if it's triggered on `Start`.
  void Start(base::OnceClosure policy_settings_and_force_installs_applied);
  void Shutdown();

  void ReinstallPlaceholderAppIfNecessary(
      const GURL& url,
      ExternallyManagedAppManager::OnceInstallCallback on_complete);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Used for handling SystemFeaturesDisableList policy. Checks if the app is
  // disabled and notifies sync_bridge_ about the current app state.
  void OnDisableListPolicyChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets system web apps disabled by SystemFeaturesDisableList policy.
  const std::set<ash::SystemWebAppType>& GetDisabledSystemWebApps() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Gets ids of web apps disabled by SystemFeaturesDisableList policy.
  const std::set<webapps::AppId>& GetDisabledWebAppsIds() const;

  // Checks if web app is disabled by SystemFeaturesDisableList policy.
  bool IsWebAppInDisabledList(const webapps::AppId& app_id) const;

  // Checks if UI mode of disabled web apps is hidden.
  bool IsDisabledAppsModeHidden() const;

  RunOnOsLoginPolicy GetUrlRunOnOsLoginPolicy(
      const webapps::AppId& app_id) const;

  void SetOnAppsSynchronizedCompletedCallbackForTesting(
      base::OnceClosure callback);
  void SetRefreshPolicySettingsCompletedCallbackForTesting(
      base::OnceClosure callback);
  void RefreshPolicySettingsForTesting();

  // Changes the manifest to conform to the WebAppInstallForceList policy.
  void MaybeOverrideManifest(content::RenderFrameHost* frame_host,
                             blink::mojom::ManifestPtr& manifest) const;

  bool IsPreventCloseEnabled(const webapps::AppId& app_id) const;

  void RefreshPolicyInstalledAppsForTesting(
      bool allow_close_and_relaunch = false);

 private:
  friend class WebAppPolicyManagerTestBase;

  struct WebAppSetting {
    bool Parse(const base::Value::Dict& dict, bool for_default_settings);

    RunOnOsLoginPolicy run_on_os_login_policy = RunOnOsLoginPolicy::kAllowed;
    bool prevent_close = false;
    bool force_unregister_os_integration = false;
  };

  struct CustomManifestValues {
    // The constructors and destructors have the "= default" implementations,
    // but they cannot be inlined.
    CustomManifestValues();
    CustomManifestValues(const CustomManifestValues&);
    ~CustomManifestValues();

    void SetName(const std::string& utf8_name);
    void SetIcon(const GURL& icon_gurl);

    std::optional<std::u16string> name;
    std::optional<std::vector<blink::Manifest::ImageResource>> icons;
  };

  void InitChangeRegistrarAndRefreshPolicy(bool enable_pwa_support);

  void RefreshPolicyInstalledApps(bool allow_close_and_relaunch = false);
  void ParsePolicySettings();
  void RefreshPolicySettings();
  void OnAppsSynchronized(
      std::map<GURL, ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<GURL, webapps::UninstallResultCode> uninstall_results);

  void ApplyPolicySettings();
  void ApplyRunOnOsLoginPolicySettings(
      base::OnceClosure policy_settings_applied_callback);
  void ApplyForceOSUnregistrationPolicySettings(
      base::OnceClosure policy_settings_applied_callback);

  void OverrideManifest(const GURL& custom_values_key,
                        blink::mojom::ManifestPtr& manifest) const;
  RunOnOsLoginPolicy GetUrlRunOnOsLoginPolicyByManifestId(
      const std::string& manifest_id) const;

  // Parses install options from a `base::Value::Dict`, which represents one
  // entry of the kWepAppInstallForceList. If the value contains a custom_name
  // or custom_icon, it is inserted into the custom_manifest_values_by_url_ map.
  // If the install_url in the entry is invalid, returns a `std::nullopt`, so
  // that installation can be skipped.
  std::optional<ExternalInstallOptions> ParseInstallPolicyEntry(
      const base::Value::Dict& entry);

  void ObserveDisabledSystemFeaturesPolicy();

  void OnDisableModePolicyChanged();

  void OnSyncPolicySettingsCommandsComplete();

  // Populates ids lists of web apps disabled by SystemFeaturesDisableList
  // policy.
  void PopulateDisabledWebAppsIdsLists();
  void OnWebAppForceInstallPolicyParsed();

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<const ash::SystemWebAppDelegateMap, DanglingUntriaged>
      system_web_apps_delegate_map_ = nullptr;
#endif
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // List of disabled system web apps, containing app types.
  std::set<ash::SystemWebAppType> disabled_system_apps_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // List of disabled system and progressive web apps, containing app ids.
  std::set<webapps::AppId> disabled_web_apps_;

  // Testing callbacks
  base::OnceClosure refresh_policy_settings_completed_;
  base::OnceClosure on_apps_synchronized_for_testing_;

  bool is_refreshing_ = false;
  bool needs_refresh_ = false;

  base::flat_map<std::string, WebAppSetting> settings_by_url_;
  base::flat_map<GURL, CustomManifestValues> custom_manifest_values_by_url_;
  WebAppSetting default_settings_;

  base::OnceClosure policy_settings_and_force_installs_applied_;


  base::WeakPtrFactory<WebAppPolicyManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_
