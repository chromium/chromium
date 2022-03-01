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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
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
class WebAppSyncBridge;
class WebAppPolicyManager;

using SystemAppDelegateMap =
    base::flat_map<SystemAppType, std::unique_ptr<SystemWebAppDelegate>>;

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
  bool IsAppEnabled(SystemAppType type);

  explicit SystemWebAppManager(Profile* profile);
  SystemWebAppManager(const SystemWebAppManager&) = delete;
  SystemWebAppManager& operator=(const SystemWebAppManager&) = delete;
  virtual ~SystemWebAppManager();

  void SetSubsystems(
      ExternallyManagedAppManager* externally_managed_app_manager,
      WebAppRegistrar* registrar,
      WebAppSyncBridge* sync_bridge,
      WebAppUiManager* ui_manager,
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
  absl::optional<AppId> GetAppIdForSystemApp(SystemAppType type) const;

  // Returns the System App Type for the given |app_id|.
  absl::optional<SystemAppType> GetSystemAppTypeForAppId(
      const AppId& app_id) const;

  // Returns the System App Delegate for the given App |type|.
  const SystemWebAppDelegate* GetSystemApp(SystemAppType type) const;

  // Returns the App Ids for all installed System Web Apps.
  std::vector<AppId> GetAppIds() const;

  // Returns whether |app_id| points to an installed System App.
  bool IsSystemWebApp(const AppId& app_id) const;

  // Perform tab-specific setup when a navigation in a System Web App is about
  // to be committed.
  void OnReadyToCommitNavigation(const AppId& app_id,
                                 content::NavigationHandle* navigation_handle);

  // Returns the SystemAppType that should capture the navigation to |url|.
  absl::optional<SystemAppType> GetCapturingSystemAppForURL(
      const GURL& url) const;

  // Returns a map of registered system app types and infos, these apps will be
  // installed on the system.
  const SystemAppDelegateMap& GetRegisteredSystemAppsForTesting() const;

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
  void SetSystemAppsForTesting(SystemAppDelegateMap system_apps);

  // Overrides the update policy. If AlwaysReinstallSystemWebApps feature is
  // enabled, this method does nothing, and system apps will be reinstalled.
  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  void ResetOnAppsSynchronizedForTesting();

  void Shutdown();

  // Get the timers. Only use this for testing.
  const std::vector<std::unique_ptr<SystemAppBackgroundTask>>&
  GetBackgroundTasksForTesting();

  const Profile* profile() const { return profile_; }

 protected:
  virtual const base::Version& CurrentVersion() const;
  virtual const std::string& CurrentLocale() const;

 private:
  // Returns the list of origin trials to enable for |url| loaded in System
  // App |type|. Returns an empty vector if the App does not specify origin
  // trials for |url|.
  const std::vector<std::string>* GetEnabledOriginTrials(
      const SystemWebAppDelegate* system_app,
      const GURL& url) const;

  void StopBackgroundTasks();

  void OnAppsSynchronized(
      bool did_force_install_apps,
      const base::TimeTicks& install_start_time,
      std::map<GURL, ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<GURL, bool> uninstall_results);
  bool ShouldForceInstallApps() const;
  void UpdateLastAttemptedInfo();
  // Returns if we have exceeded the number of retry attempts allowed for this
  // version.
  bool CheckAndIncrementRetryAttempts();

  void RecordSystemWebAppInstallResults(
      const std::map<GURL, ExternallyManagedAppManager::InstallResult>&
          install_results) const;

  void RecordSystemWebAppInstallDuration(
      const base::TimeDelta& time_duration) const;

  void StartBackgroundTasks() const;

  raw_ptr<Profile> profile_;

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;
  std::unique_ptr<base::OneShotEvent> on_tasks_started_;

  bool shutting_down_ = false;

  std::string install_result_per_profile_histogram_name_;

  UpdatePolicy update_policy_;

  SystemAppDelegateMap system_app_delegates_;

  const raw_ptr<PrefService> pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  raw_ptr<ExternallyManagedAppManager> externally_managed_app_manager_ =
      nullptr;

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;

  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;

  raw_ptr<WebAppUiManager> ui_manager_ = nullptr;

  raw_ptr<WebAppPolicyManager> web_app_policy_manager_ = nullptr;

  std::vector<std::unique_ptr<SystemAppBackgroundTask>> tasks_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
