// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_H_

#include <map>
#include <memory>
#include <ostream>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/gurl.h"

namespace webapps {
enum class InstallResultCode;
}

namespace web_app {

class WebAppRegistrar;
class WebAppInstallFinalizer;
class WebAppCommandScheduler;
class WebAppUiManager;
class WebAppSyncBridge;

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
                           absl::optional<AppId> app_id = absl::nullopt,
                           bool did_uninstall_and_replace = false);
    InstallResult(const InstallResult&);
    ~InstallResult();

    bool operator==(const InstallResult& other) const;

    webapps::InstallResultCode code;
    absl::optional<AppId> app_id;
    bool did_uninstall_and_replace = false;
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
      base::RepeatingCallback<void(const GURL& install_url, bool succeeded)>;
  using SynchronizeCallback = base::OnceCallback<void(
      std::map<GURL /*install_url*/, InstallResult> install_results,
      std::map<GURL /*install_url*/, bool /*succeeded*/> uninstall_results)>;

  ExternallyManagedAppManager();
  ExternallyManagedAppManager(const ExternallyManagedAppManager&) = delete;
  ExternallyManagedAppManager& operator=(const ExternallyManagedAppManager&) =
      delete;
  virtual ~ExternallyManagedAppManager();

  void SetSubsystems(WebAppRegistrar* registrar,
                     WebAppUiManager* ui_manager,
                     WebAppInstallFinalizer* finalizer,
                     WebAppCommandScheduler* command_scheduler,
                     WebAppSyncBridge* sync_bridge);

  // Queues an installation operation with the highest priority. Essentially
  // installing the app immediately if there are no ongoing operations or
  // installing the app right after the current operation finishes. Runs its
  // callback with the URL in |install_options| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before. Should only be used in
  // response to a user action e.g. the user clicked an install button.
  virtual void InstallNow(ExternalInstallOptions install_options,
                          OnceInstallCallback callback) = 0;

  // Queues an installation operation the end of current tasks. Runs its
  // callback with the URL in |install_options| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before.
  virtual void Install(ExternalInstallOptions install_options,
                       OnceInstallCallback callback) = 0;

  // Adds a task to the queue of operations for each ExternalInstallOptions in
  // |install_options_list|. Runs |callback| with the URL of the corresponding
  // ExternalInstallOptions in |install_options_list| and with the id of the
  // installed app or an empty string if the installation fails. Runs |callback|
  // for every completed installation - whether or not the installation actually
  // succeeded.
  virtual void InstallApps(
      std::vector<ExternalInstallOptions> install_options_list,
      const RepeatingInstallCallback& callback) = 0;

  // Adds a task to the queue of operations for each GURL in
  // |uninstall_urls|. Runs |callback| with the URL of the corresponding
  // app in |uninstall_urls| and with a bool indicating whether or not the
  // uninstall succeeded. Runs |callback| for every completed uninstallation -
  // whether or not the uninstallation actually succeeded.
  virtual void UninstallApps(std::vector<GURL> uninstall_urls,
                             ExternalInstallSource install_source,
                             const UninstallCallback& callback) = 0;

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

  virtual void Shutdown() = 0;

 protected:
  WebAppRegistrar* registrar() { return registrar_; }
  WebAppUiManager* ui_manager() { return ui_manager_; }
  WebAppInstallFinalizer* finalizer() { return finalizer_; }
  WebAppCommandScheduler* command_scheduler() { return command_scheduler_; }
  WebAppSyncBridge* sync_bridge() { return sync_bridge_; }

  virtual void OnRegistrationFinished(const GURL& launch_url,
                                      RegistrationResultCode result);

  base::OnceClosure registrations_complete_callback_;

 private:
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
    std::map<GURL, bool> uninstall_results;
  };

  void InstallForSynchronizeCallback(
      ExternalInstallSource source,
      const GURL& install_url,
      ExternallyManagedAppManager::InstallResult result);
  void UninstallForSynchronizeCallback(ExternalInstallSource source,
                                       const GURL& install_url,
                                       bool succeeded);
  void ContinueOrCompleteSynchronization(ExternalInstallSource source);

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<WebAppUiManager, DanglingUntriaged> ui_manager_ = nullptr;
  raw_ptr<WebAppInstallFinalizer> finalizer_ = nullptr;
  raw_ptr<WebAppCommandScheduler, DanglingUntriaged> command_scheduler_ =
      nullptr;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;

  base::flat_map<ExternalInstallSource, SynchronizeRequest>
      synchronize_requests_;

  RegistrationCallback registration_callback_;

  base::WeakPtrFactory<ExternallyManagedAppManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_H_
