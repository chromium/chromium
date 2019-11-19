// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "url/gurl.h"

namespace web_app {

enum class InstallResultCode;

class AppRegistrar;
class AppShortcutManager;
class InstallFinalizer;
class WebAppUiManager;

enum class RegistrationResultCode { kSuccess, kAlreadyRegistered, kTimeout };

// PendingAppManager installs, uninstalls, and updates apps.
//
// Implementations of this class should perform each set of operations serially
// in the order in which they arrive. For example, if an uninstall request gets
// queued while an update request for the same app is pending, implementations
// should wait for the update request to finish before uninstalling the app.
class PendingAppManager {
 public:
  using OnceInstallCallback =
      base::OnceCallback<void(const GURL& app_url, InstallResultCode code)>;
  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const GURL& app_url,
                                   InstallResultCode code)>;
  using RegistrationCallback =
      base::RepeatingCallback<void(const GURL& launch_url,
                                   RegistrationResultCode code)>;
  using UninstallCallback =
      base::RepeatingCallback<void(const GURL& app_url, bool succeeded)>;
  using SynchronizeCallback =
      base::OnceCallback<void(std::map<GURL, InstallResultCode> install_results,
                              std::map<GURL, bool> uninstall_results)>;

  PendingAppManager();
  virtual ~PendingAppManager();

  void SetSubsystems(AppRegistrar* registrar,
                     AppShortcutManager* shortcut_manager,
                     WebAppUiManager* ui_manager,
                     InstallFinalizer* finalizer);

  // Queues an installation operation with the highest priority. Essentially
  // installing the app immediately if there are no ongoing operations or
  // installing the app right after the current operation finishes. Runs its
  // callback with the URL in |install_options| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before. Should only be used in
  // response to a user action e.g. the user clicked an install button.
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

  virtual void Shutdown() = 0;

 protected:
  AppRegistrar* registrar() { return registrar_; }
  AppShortcutManager* shortcut_manager() { return shortcut_manager_; }
  WebAppUiManager* ui_manager() { return ui_manager_; }
  InstallFinalizer* finalizer() { return finalizer_; }

  virtual void OnRegistrationFinished(const GURL& launch_url,
                                      RegistrationResultCode result);

 private:
  struct SynchronizeRequest {
    SynchronizeRequest(SynchronizeCallback callback, int remaining_requests);
    ~SynchronizeRequest();

    SynchronizeRequest& operator=(SynchronizeRequest&&);
    SynchronizeRequest(SynchronizeRequest&& other);

    SynchronizeCallback callback;
    int remaining_requests;
    std::map<GURL, InstallResultCode> install_results;
    std::map<GURL, bool> uninstall_results;

   private:
    DISALLOW_COPY_AND_ASSIGN(SynchronizeRequest);
  };

  void InstallForSynchronizeCallback(ExternalInstallSource source,
                                     const GURL& app_url,
                                     InstallResultCode code);
  void UninstallForSynchronizeCallback(ExternalInstallSource source,
                                       const GURL& app_url,
                                       bool succeeded);
  void OnAppSynchronized(ExternalInstallSource source, const GURL& app_url);

  AppRegistrar* registrar_ = nullptr;
  AppShortcutManager* shortcut_manager_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;
  InstallFinalizer* finalizer_ = nullptr;

  base::flat_map<ExternalInstallSource, SynchronizeRequest>
      synchronize_requests_;

  RegistrationCallback registration_callback_;

  base::WeakPtrFactory<PendingAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
