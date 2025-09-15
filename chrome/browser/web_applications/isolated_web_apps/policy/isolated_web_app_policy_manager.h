// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "net/base/backoff_entry.h"

namespace web_app {

// Controls whether we attempt to fetch latest component data before processing
// the policy for the first time.
BASE_DECLARE_FEATURE(kIwaPolicyManagerOnDemandComponentUpdate);

// This class is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager
    : public IwaKeyDistributionInfoProvider::Observer {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void SetOnInstallTaskCompletedCallbackForTesting(
      base::RepeatingCallback<void(web_package::SignedWebBundleId,
                                   IwaInstaller::Result)> callback);

  static std::vector<IsolatedWebAppExternalInstallOptions>
  GetIwaInstallForceList(const Profile& profile);

  explicit IsolatedWebAppPolicyManager(Profile* profile);

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;
  ~IsolatedWebAppPolicyManager() override;

  void Start(base::OnceClosure on_started_callback);
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  base::Value GetDebugValue() const;

  static void RemoveDelayForBundleCleanupForTesting();

 private:
  void StartImpl();

  void ConfigureObserversOnSessionStart();
  void CleanupAndProcessPolicyOnSessionStart();
  void ProcessPolicy();
  void DoProcessPolicy(AllAppsLock& lock, base::Value::Dict& debug_info);
  void OnPolicyProcessed();

  void LogAddPolicyInstallSourceResult(
      web_package::SignedWebBundleId web_bundle_id);

  void LogRemoveInstallSourceResult(
      web_package::SignedWebBundleId web_bundle_id,
      WebAppManagement::Type source,
      webapps::UninstallResultCode uninstall_code);

  void OnInstallTaskCompleted(
      web_package::SignedWebBundleId web_bundle_id,
      base::RepeatingCallback<void(IwaInstaller::Result)> callback,
      IwaInstaller::Result install_result);
  void OnAllInstallTasksCompleted(
      std::vector<IwaInstaller::Result> install_results);

  void MaybeStartNextInstallTask();

  void CleanupOrphanedBundles();

  void OnPolicyChanged();

  // IwaKeyDistributionInfoProvider::Observer:
  void OnComponentUpdateSuccess(bool is_preloaded) override;

  // Keeps track of the last few processing logs for debugging purposes.
  // Automatically discards older logs to keep at most `kMaxEntries`.
  class ProcessLogs {
   public:
    static constexpr size_t kMaxEntries = 10;

    ProcessLogs();
    ~ProcessLogs();

    void AppendCompletedStep(base::Value::Dict log);

    base::Value ToDebugValue() const;

   private:
    base::circular_deque<base::Value::Dict> logs_;
  };

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  ProcessLogs process_logs_;

  bool reprocess_policy_needed_ = false;
  bool policy_is_being_processed_ = false;
  base::Value::Dict current_process_log_;

  net::BackoffEntry install_retry_backoff_entry_;

  // We must execute install tasks in a queue, because each task uses a
  // `WebContents`, and installing an unbound number of apps in parallel would
  // use too many resources.
  base::queue<std::unique_ptr<IwaInstaller>> install_tasks_;

  base::ScopedObservation<IwaKeyDistributionInfoProvider,
                          IwaKeyDistributionInfoProvider::Observer>
      key_distribution_info_observation_{this};

  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_ptr_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
