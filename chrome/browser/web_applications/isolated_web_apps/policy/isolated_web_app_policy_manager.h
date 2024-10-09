// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"

namespace web_app {

// This class is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit IsolatedWebAppPolicyManager(Profile* profile);

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;
  ~IsolatedWebAppPolicyManager();

  void Start(base::OnceClosure on_started_callback);
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  base::Value GetDebugValue() const;

 private:
  void CleanupAndProcessPolicyOnSessionStart();
  int GetPendingInitCount();
  void SetPendingInitCount(int pending_count);
  void ProcessPolicy(base::OnceClosure finished_closure);
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

  void CleanupOrphanedBundles(base::OnceClosure finished_closure);

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
  base::OnceClosure on_started_callback_;

  bool reprocess_policy_needed_ = false;
  bool policy_is_being_processed_ = false;
  base::Value::Dict current_process_log_;

  net::BackoffEntry install_retry_backoff_entry_;

  // We must execute install tasks in a queue, because each task uses a
  // `WebContents`, and installing an unbound number of apps in parallel would
  // use too many resources.
  base::queue<std::unique_ptr<IwaInstaller>> install_tasks_;

  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_ptr_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
