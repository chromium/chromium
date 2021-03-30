// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Version;
}

namespace content {
class NavigationHandle;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class Profile;

namespace web_app {

class WebAppUiManager;
class OsIntegrationManager;
class AppRegistryController;
class WebAppPolicyManager;

using OriginTrialsMap = std::map<url::Origin, std::vector<std::string>>;
using WebApplicationInfoFactory =
    base::RepeatingCallback<std::unique_ptr<WebApplicationInfo>()>;

// The configuration options for a System App.
struct SystemAppInfo {
  // When installing via a WebApplicationInfo, the url is never loaded. It's
  // needed only for various legacy reasons, maps for tracking state, and
  // generating the AppId and things of that nature.
  SystemAppInfo(const std::string& internal_name,
                const GURL& install_url,
                const WebApplicationInfoFactory& info_factory);
  SystemAppInfo(const SystemAppInfo& other);
  ~SystemAppInfo();

  // A developer-friendly name for, among other things, reporting metrics and
  // interacting with tast tests. It should follow PascalCase convention, and
  // have a corresponding entry in WebAppSystemAppInternalName histogram
  // suffixes. The internal name shouldn't be changed afterwards.
  std::string internal_name;

  // The URL that the System App will be installed from.
  GURL install_url;

  // If specified, the apps in |uninstall_and_replace| will have their data
  // migrated to this System App.
  std::vector<AppId> uninstall_and_replace;

  // Minimum window size in DIPs. Empty if the app does not have a minimum.
  // TODO(https://github.com/w3c/manifest/issues/436): Replace with PWA manifest
  // properties for window size.
  gfx::Size minimum_window_size;

  // If set, we allow only a single window for this app.
  bool single_window = true;

  // If set, when the app is launched through the File Handling Web API, we will
  // include the file's directory in window.launchQueue as the first value.
  bool include_launch_directory = false;

  // Map from origin to enabled origin trial names for this app. For example,
  // "chrome://sample-web-app/" to ["Frobulate"]. If set, we will enable the
  // given origin trials when the corresponding origin is loaded in the app.
  OriginTrialsMap enabled_origin_trials;

  // Resource Ids for additional search terms.
  std::vector<int> additional_search_terms;

  // If set to false, this app will be hidden from the Chrome OS app launcher.
  bool show_in_launcher = true;

  // If set to false, this app will be hidden from the Chrome OS search.
  bool show_in_search = true;

  // If set to true, navigations (e.g. Omnibox URL, anchor link) to this app
  // will open in the app's window instead of the navigation's context (e.g.
  // browser tab).
  bool capture_navigations = false;

  // If set to false, the app will non-resizeable.
  bool is_resizeable = true;

  // If set to false, the surface of app will can be non-maximizable.
  bool is_maximizable = true;

  // If set to false, the app will not have the reload button in minimal ui
  // mode.
  bool should_have_reload_button_in_minimal_ui = true;

  // If set, allows the app to close the window through scripts, for example
  // using `window.close()`.
  bool allow_scripts_to_close_windows = false;

  WebApplicationInfoFactory app_info_factory;

  // Setup information to drive a background task.
  base::Optional<SystemAppBackgroundTaskInfo> timer_info;
};

// Installs, uninstalls, and updates System Web Apps.
// System Web Apps are built-in, highly-privileged Web Apps for Chrome OS. They
// have access to more APIs and are part of the Chrome OS image.
class SystemWebAppManager {
 public:
  // Policy for when the SystemWebAppManager will update apps/install new apps.
  enum class UpdatePolicy {
    // Update every system start.
    kAlwaysUpdate,
    // Update when the Chrome version number changes.
    kOnVersionChange,
  };

  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.System";
  static constexpr char kInstallDurationHistogramName[] =
      "Webapp.SystemApps.FreshInstallDuration";

  // Returns whether the given app type is enabled.
  static bool IsAppEnabled(SystemAppType type);

  explicit SystemWebAppManager(Profile* profile);
  SystemWebAppManager(const SystemWebAppManager&) = delete;
  SystemWebAppManager& operator=(const SystemWebAppManager&) = delete;
  virtual ~SystemWebAppManager();

  void SetSubsystems(PendingAppManager* pending_app_manager,
                     AppRegistrar* registrar,
                     AppRegistryController* registry_controller,
                     WebAppUiManager* ui_manager,
                     OsIntegrationManager* os_integration_manager,
                     WebAppPolicyManager* web_app_policy_manager);

  void Start();

  // The SystemWebAppManager is disabled in browser tests by default because it
  // pollutes the startup state (several tests expect the Extensions state to be
  // clean).
  //
  // Call this to install apps for SystemWebApp specific tests, e.g if a test
  // needs to open OS Settings.
  //
  // This can also be called multiple times to simulate reinstallation from
  // system restart, e.g.
  void InstallSystemAppsForTesting();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the app id for the given System App |type|.
  base::Optional<AppId> GetAppIdForSystemApp(SystemAppType type) const;

  // Returns the System App Type for the given |app_id|.
  base::Optional<SystemAppType> GetSystemAppTypeForAppId(AppId app_id) const;

  // Returns the App Ids for all installed System Web Apps.
  std::vector<AppId> GetAppIds() const;

  // Returns whether |app_id| points to an installed System App.
  bool IsSystemWebApp(const AppId& app_id) const;

  // Returns whether the given System App |type| should use a single window.
  bool IsSingleWindow(SystemAppType type) const;

  // Returns whether the given System App |type| should get launch directory in
  // launch parameter.
  bool AppShouldReceiveLaunchDirectory(SystemAppType type) const;

  // Perform tab-specific setup when a navigation in a System Web App is about
  // to be committed.
  void OnReadyToCommitNavigation(const AppId& app_id,
                                 content::NavigationHandle* navigation_handle);

  // Returns terms to be used when searching for the app.
  std::vector<std::string> GetAdditionalSearchTerms(SystemAppType type) const;

  // Returns whether the app should be shown in the launcher.
  bool ShouldShowInLauncher(SystemAppType type) const;

  // Returns whether the app should be shown in search.
  bool ShouldShowInSearch(SystemAppType type) const;

  // Returns whether the app should be resizeable.
  bool IsResizeableWindow(SystemAppType type) const;

  // Returns whether the surface of app can be maximizable.
  bool IsMaximizableWindow(SystemAppType type) const;

  // Returns whether the app should have the reload button in minimal ui mode.
  bool ShouldHaveReloadButtonInMinimalUi(SystemAppType type) const;

  // Returns whether the app is allowed to close the window through scripts.
  bool AllowScriptsToCloseWindows(SystemAppType type) const;

  // Returns the SystemAppType that should capture the navigation to |url|.
  base::Optional<SystemAppType> GetCapturingSystemAppForURL(
      const GURL& url) const;

  // Returns the minimum window size for |app_id| or an empty size if the app
  // doesn't specify a minimum.
  gfx::Size GetMinimumWindowSize(const AppId& app_id) const;

  // Returns a map of registered system app types and infos, these apps will be
  // installed on the system.
  const base::flat_map<SystemAppType, SystemAppInfo>&
  GetRegisteredSystemAppsForTesting() const;

  const base::OneShotEvent& on_apps_synchronized() const {
    return *on_apps_synchronized_;
  }

  // Return the OneShotEvent that is fired after all of the background tasks
  // have started and their timers become active.
  const base::OneShotEvent& on_tasks_started() const {
    return *on_tasks_started_;
  }

  // This call will override default System Apps configuration. You should call
  // Start() after this call to install |system_apps|.
  void SetSystemAppsForTesting(
      base::flat_map<SystemAppType, SystemAppInfo> system_apps);

  // Overrides the update policy. If AlwaysReinstallSystemWebApps feature is
  // enabled, this method does nothing, and system apps will be reinstalled.
  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  void ResetOnAppsSynchronizedForTesting();

  void Shutdown();

  // Get the timers. Only use this for testing.
  const std::vector<std::unique_ptr<SystemAppBackgroundTask>>&
  GetBackgroundTasksForTesting();

 protected:
  virtual const base::Version& CurrentVersion() const;
  virtual const std::string& CurrentLocale() const;

 private:
  // Returns the list of origin trials to enable for |url| loaded in System App
  // |type|. Returns nullptr if the App does not specify origin trials for
  // |url|.
  const std::vector<std::string>* GetEnabledOriginTrials(SystemAppType type,
                                                         const GURL& url);

  bool AppHasFileHandlingOriginTrial(SystemAppType type);

  void StopBackgroundTasks();

  void OnAppsSynchronized(
      bool did_force_install_apps,
      const base::TimeTicks& install_start_time,
      std::map<GURL, PendingAppManager::InstallResult> install_results,
      std::map<GURL, bool> uninstall_results);
  bool ShouldForceInstallApps() const;
  void UpdateLastAttemptedInfo();
  // Returns if we have exceeded the number of retry attempts allowed for this
  // version.
  bool CheckAndIncrementRetryAttempts();

  void RecordSystemWebAppInstallResults(
      const std::map<GURL, PendingAppManager::InstallResult>& install_results)
      const;

  void RecordSystemWebAppInstallDuration(
      const base::TimeDelta& time_duration) const;

  void StartBackgroundTasks() const;

  Profile* profile_;

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;
  std::unique_ptr<base::OneShotEvent> on_tasks_started_;

  bool shutting_down_ = false;

  std::string install_result_per_profile_histogram_name_;

  UpdatePolicy update_policy_;

  base::flat_map<SystemAppType, SystemAppInfo> system_app_infos_;

  PrefService* const pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_ = nullptr;

  AppRegistrar* registrar_ = nullptr;

  AppRegistryController* registry_controller_ = nullptr;

  WebAppUiManager* ui_manager_ = nullptr;

  OsIntegrationManager* os_integration_manager_ = nullptr;

  WebAppPolicyManager* web_app_policy_manager_ = nullptr;

  std::vector<std::unique_ptr<SystemAppBackgroundTask>> tasks_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
