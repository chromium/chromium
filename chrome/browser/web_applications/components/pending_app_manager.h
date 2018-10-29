// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "url/gurl.h"

namespace web_app {

enum class InstallResultCode;
enum class InstallSource;
enum class LaunchContainer;

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
  using UninstallCallback =
      base::RepeatingCallback<void(const GURL& app_url, bool succeeded)>;

  struct AppInfo {
    static const bool kDefaultCreateShortcuts;
    static const bool kDefaultOverridePreviousUserUninstall;
    static const bool kDefaultBypassServiceWorkerCheck;
    static const bool kDefaultRequireManifest;

    AppInfo(GURL url,
            LaunchContainer launch_container,
            InstallSource install_source,
            bool create_shortcuts = kDefaultCreateShortcuts,
            bool override_previous_user_uninstall =
                kDefaultOverridePreviousUserUninstall,
            bool bypass_service_worker_check = kDefaultBypassServiceWorkerCheck,
            bool require_manifest = kDefaultRequireManifest);
    AppInfo(AppInfo&& other);
    ~AppInfo();

    std::unique_ptr<AppInfo> Clone() const;

    bool operator==(const AppInfo& other) const;

    const GURL url;
    const LaunchContainer launch_container;
    const InstallSource install_source;
    const bool create_shortcuts;
    const bool override_previous_user_uninstall;

    // This must only be used by pre-installed default or system apps that are
    // valid PWAs if loading the real service worker is too costly to verify
    // programmatically.
    const bool bypass_service_worker_check;

    // This should be used for installing all default apps so that good metadata
    // is ensured.
    const bool require_manifest;

   private:
    DISALLOW_COPY_AND_ASSIGN(AppInfo);
  };

  PendingAppManager();
  virtual ~PendingAppManager();

  // Queues an installation operation with the highest priority. Essentially
  // installing the app immediately if there are no ongoing operations or
  // installing the app right after the current operation finishes. Runs its
  // callback with the URL in |app_to_install| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before. Should only be used in
  // response to a user action e.g. the user clicked an install button.
  virtual void Install(AppInfo app_to_install,
                       OnceInstallCallback callback) = 0;

  // Adds |apps_to_install| to the queue of operations. Runs |callback|
  // with the URL of the corresponding AppInfo in |apps_to_install| and with the
  // id of the installed app or an empty string if the installation fails. Runs
  // |callback| for every completed installation - whether or not the
  // installation actually succeeded.
  virtual void InstallApps(std::vector<AppInfo> apps_to_install,
                           const RepeatingInstallCallback& callback) = 0;

  // Adds |apps_to_uninstall| to the queue of operations. Runs |callback|
  // with the URL of the corresponding app in |apps_to_install| and with a
  // bool indicating whether or not the uninstall succeeded. Runs |callback|
  // for every completed uninstallation - whether or not the uninstallation
  // actually succeeded.
  virtual void UninstallApps(std::vector<GURL> apps_to_uninstall,
                             const UninstallCallback& callback) = 0;

  // Returns the URLs of those apps installed from |install_source|.
  virtual std::vector<GURL> GetInstalledAppUrls(
      InstallSource install_source) const = 0;

  // Installs |desired_apps| and uninstalls any apps in
  // GetInstalledAppUrls(install_source) that are not in |desired_apps|'s URLs.
  //
  // All apps in |desired_apps| should have |install_source| as their source.
  //
  // Note that this returns after queueing work (installation and
  // uninstallation) to be done. It does not wait until that work is complete.
  void SynchronizeInstalledApps(std::vector<AppInfo> desired_apps,
                                InstallSource install_source);

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
