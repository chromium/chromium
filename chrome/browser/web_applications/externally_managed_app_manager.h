// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_H_

#include <iosfwd>
#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace base {
class Value;
}

namespace webapps {
enum class InstallResultCode;
enum class UninstallResultCode;
class WebAppUrlLoader;
}

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class AllAppsLock;
class ExternallyManagedAppInstallTask;
class ExternallyManagedAppRegistrationTaskBase;
class WebAppDataRetriever;
class WebAppProvider;

enum class RegistrationResultCode { kSuccess, kAlreadyRegistered, kTimeout };

// ExternallyManagedAppManager installs, uninstalls, and updates apps that are
// externally managed. This means that they are not installed by the user, but
// instead through a different system (enterprise installs, device default
// installs, etc). See ExternalInstallSource for all of the different externally
// managed app types. Typically there is one "manager" class per externally
// managed app type that figures out the list of apps to have installed, and
// that class will call SynchronizeApps for a given external install type.
//
// Implementations of this class should perform each set of operations serially
// in the order in which they arrive. For example, if an uninstall request gets
// queued while an update request for the same app is pending, implementations
// should wait for the update request to finish before uninstalling the app.
//
// This class also supports installing a "placeholder" app by the
// |install_placeholder| in ExternalInstallOptions. This placeholder app is
// installed if the install url given fails to fully load so a manifest cannot
// be resolved, and a placeholder app is installed instead. Every time the user
// navigates a page, if that page is ever a URL that a placeholder app is
// 'holding' (as the app failed to load originally), then the install is
// re-initiated, and if successful, the placeholder app is removed.
class ExternallyManagedAppManager {
 public:
  struct InstallResult {
    InstallResult();
    explicit InstallResult(webapps::InstallResultCode code,
                           std::optional<webapps::AppId> app_id = std::nullopt,
                           bool did_uninstall_and_replace = false);
    InstallResult(const InstallResult&);
    ~InstallResult();

    bool operator==(const InstallResult& other) const;

    webapps::InstallResultCode code;
    std::optional<webapps::AppId> app_id;
    bool did_uninstall_and_replace = false;
    // When adding fields, please update the `==` and `<<` operators to include
    // the new field.
  };

  using OnceInstallCallback =
      base::OnceCallback<void(const GURL& install_url, InstallResult result)>;
  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const GURL& install_url,
                                   InstallResult result)>;
  using RegistrationCallback =
      base::RepeatingCallback<void(const GURL& launch_url,
                                   RegistrationResultCode code)>;
  using UninstallCallback =
      base::RepeatingCallback<void(const GURL& install_url,
                                   webapps::UninstallResultCode)>;
  using SynchronizeCallback = base::OnceCallback<void(
      std::map<GURL /*install_url*/, InstallResult> install_results,
      std::map<GURL /*install_url*/, webapps::UninstallResultCode>
          uninstall_results)>;

  explicit ExternallyManagedAppManager(Profile* profile);
  ExternallyManagedAppManager(const ExternallyManagedAppManager&) = delete;
  ExternallyManagedAppManager& operator=(const ExternallyManagedAppManager&) =
      delete;
  virtual ~ExternallyManagedAppManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  // Queues an installation operation with the highest priority. Essentially
  // installing the app immediately if there are no ongoing operations or
  // installing the app right after the current operation finishes. Runs its
  // callback with the URL in |install_options| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before. Should only be used in
  // response to a user action e.g. the user clicked an install button.
  virtual void InstallNow(ExternalInstallOptions install_options,
                          OnceInstallCallback callback);

  // Queues an installation operation the end of current tasks. Runs its
  // callback with the URL in |install_options| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before.
  virtual void Install(ExternalInstallOptions install_options,
                       OnceInstallCallback callback);

  // Adds a task to the queue of operations for each ExternalInstallOptions in
  // |install_options_list|. Runs |callback| with the URL of the corresponding
  // ExternalInstallOptions in |install_options_list| and with the id of the
  // installed app or an empty string if the installation fails. Runs |callback|
  // for every completed installation - whether or not the installation actually
  // succeeded.
  virtual void InstallApps(
      std::vector<ExternalInstallOptions> install_options_list,
      const RepeatingInstallCallback& callback);

  // Adds a task to the queue of operations for each GURL in
  // |uninstall_urls|. Runs |callback| with the URL of the corresponding
  // app in |uninstall_urls| and with a bool indicating whether or not the
  // uninstall succeeded. Runs |callback| for every completed uninstallation -
  // whether or not the uninstallation actually succeeded.
  virtual void UninstallApps(std::vector<GURL> uninstall_urls,
                             ExternalInstallSource install_source,
                             const UninstallCallback& callback);

  // Installs an app for each ExternalInstallOptions in
  // |desired_apps_install_options| and uninstalls any apps in
  // GetInstalledAppUrls(install_source) that are not in
  // |desired_apps_install_options|'s URLs.
  //
  // All apps in |desired_apps_install_options| should have |install_source| as
  // their source.
  //
  // Once all installs/uninstalls are complete, |callback| will be run with the
  // success/failure status of the synchronization.
  //
  // Note that this returns after queueing work (installation and
  // uninstallation) to be done. It does not wait until that work is complete.
  void SynchronizeInstalledApps(
      std::vector<ExternalInstallOptions> desired_apps_install_options,
      ExternalInstallSource install_source,
      SynchronizeCallback callback);

  void SetRegistrationCallbackForTesting(RegistrationCallback callback);
  void ClearRegistrationCallbackForTesting();
  void SetRegistrationsCompleteCallbackForTesting(base::OnceClosure callback);
  void ClearSynchronizeRequestsForTesting();

  void Shutdown();

  // TODO(http://b/283521737): Remove this and use WebContentsManager.
  void SetUrlLoaderForTesting(
      std::unique_ptr<webapps::WebAppUrlLoader> url_loader);
  // TODO(http://b/283521737): Remove this and use WebContentsManager.
  void SetDataRetrieverFactoryForTesting(
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()> factory);

 protected:
  virtual void ReleaseWebContents();

  virtual std::unique_ptr<ExternallyManagedAppInstallTask>
  CreateInstallationTask(ExternalInstallOptions install_options);

  virtual std::unique_ptr<ExternallyManagedAppRegistrationTaskBase>
  CreateRegistration(GURL install_url,
                     const base::TimeDelta registration_timeout);

  virtual void OnRegistrationFinished(const GURL& launch_url,
                                      RegistrationResultCode result);

  Profile* profile() { return profile_; }

  raw_ptr<WebAppProvider> provider_ = nullptr;

 private:
  struct TaskAndCallback;

  struct SynchronizeRequest {
    SynchronizeRequest(SynchronizeCallback callback,
                       std::vector<ExternalInstallOptions> pending_installs,
                       int remaining_uninstall_requests);
    SynchronizeRequest(const SynchronizeRequest&) = delete;
    SynchronizeRequest& operator=(const SynchronizeRequest&) = delete;
    ~SynchronizeRequest();

    SynchronizeRequest& operator=(SynchronizeRequest&&);
    SynchronizeRequest(SynchronizeRequest&& other);

    SynchronizeCallback callback;
    int remaining_install_requests;
    std::vector<ExternalInstallOptions> pending_installs;
    int remaining_uninstall_requests;
    std::map<GURL, InstallResult> install_results;
    std::map<GURL, webapps::UninstallResultCode> uninstall_results;
  };

  void SynchronizeInstalledAppsOnLockAcquired(
      std::vector<ExternalInstallOptions> desired_apps_install_options,
      ExternalInstallSource install_source,
      SynchronizeCallback callback,
      AllAppsLock& lock,
      base::Value::Dict& debug_value);

  void InstallForSynchronizeCallback(
      ExternalInstallSource source,
      const GURL& install_url,
      ExternallyManagedAppManager::InstallResult result);
  void UninstallForSynchronizeCallback(ExternalInstallSource source,
                                       const GURL& install_url,
                                       webapps::UninstallResultCode code);
  void ContinueSynchronization(ExternalInstallSource source);
  void CompleteSynchronization(ExternalInstallSource source);

  void PostMaybeStartNext();

  void MaybeStartNext();
  void MaybeStartNextOnLockAcquired(AllAppsLock& lock,
                                    base::Value::Dict& debug_value);

  void StartInstallationTask(
      std::unique_ptr<TaskAndCallback> task,
      std::optional<webapps::AppId> installed_placeholder_app_id);

  bool RunNextRegistration();

  void CreateWebContentsIfNecessary();

  void OnInstalled(ExternallyManagedAppManager::InstallResult result);

  void MaybeEnqueueServiceWorkerRegistration(
      const ExternalInstallOptions& install_options);

  bool IsShuttingDown();

  base::OnceClosure registrations_complete_callback_;

  const raw_ptr<Profile> profile_;

  bool is_in_shutdown_ = false;

  base::flat_map<ExternalInstallSource, SynchronizeRequest>
      synchronize_requests_;

  // unique_ptr so that it can be replaced in tests.
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  // Allows tests to set the data retriever for install tasks.
  base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>
      data_retriever_factory_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<TaskAndCallback> current_install_;

  base::circular_deque<std::unique_ptr<TaskAndCallback>> pending_installs_;

  std::unique_ptr<ExternallyManagedAppRegistrationTaskBase>
      current_registration_;

  using UrlAndTimeout = std::tuple<GURL, const base::TimeDelta>;
  base::circular_deque<UrlAndTimeout> pending_registrations_;

  RegistrationCallback registration_callback_;

  base::WeakPtrFactory<ExternallyManagedAppManager> weak_ptr_factory_{this};
};

// For logging and testing purposes.
std::ostream& operator<<(
    std::ostream& out,
    const ExternallyManagedAppManager::InstallResult& install_result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_H_
