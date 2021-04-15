// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

class Profile;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
}

namespace web_app {

enum class InstallResultCode;
class InstallFinalizer;
class AppRegistrar;
class OsIntegrationManager;

// TODO(loyso): Rework this interface. Unify the API and merge similar
// InstallWebAppZZZZ functions.
class InstallManager {
 public:
  // |app_id| may be empty on failure.
  using OnceInstallCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;
  using OnceUninstallCallback =
      base::OnceCallback<void(const AppId& app_id, bool uninstalled)>;

  // Callback used to indicate whether a user has accepted the installation of a
  // web app.
  using WebAppInstallationAcceptanceCallback =
      base::OnceCallback<void(bool user_accepted,
                              std::unique_ptr<WebApplicationInfo>)>;

  // Callback to show the WebApp installation confirmation bubble in UI.
  // |web_app_info| is the WebApplicationInfo to be installed.
  using WebAppInstallDialogCallback = base::OnceCallback<void(
      content::WebContents* initiator_web_contents,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      WebAppInstallationAcceptanceCallback acceptance_callback)>;

  enum class InstallableCheckResult {
    kNotInstallable,
    kInstallable,
    kAlreadyInstalled,
  };
  // Callback with the result of manifest check.
  // |web_contents| owns the WebContents that was used to check for a manifest.
  // |app_id| will be present iff already installed.
  using WebAppManifestCheckCallback = base::OnceCallback<void(
      std::unique_ptr<content::WebContents> web_contents,
      InstallableCheckResult result,
      base::Optional<AppId> app_id)>;

  // Checks a WebApp installability (service worker check optional), retrieves
  // manifest and icons and then performs the actual installation.
  virtual void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      bool bypass_service_worker_check,
      webapps::WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) = 0;

  // Infers WebApp info from the blink renderer process and then retrieves a
  // manifest in a way similar to |InstallWebAppFromManifest|. If the manifest
  // is incomplete or missing, the inferred info is used. |force_shortcut_app|
  // forces the creation of a shortcut app instead of a PWA even if installation
  // is available.
  virtual void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      webapps::WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) = 0;

  // See related ExternalInstallOptions struct and
  // ConvertExternalInstallOptionsToParams function.
  struct InstallParams {
    InstallParams();
    ~InstallParams();
    InstallParams(const InstallParams&);

    DisplayMode user_display_mode = DisplayMode::kUndefined;

    // URL to be used as start_url if manifest is unavailable.
    GURL fallback_start_url;

    // // Setting this field will force the webapp to have a manifest id, which
    // will result in a different AppId than if it isn't set. Currently here
    // to support forwards compatibility with future sync entities..
    base::Optional<std::string> override_manifest_id;

    // App name to be used if manifest is unavailable.
    base::Optional<std::u16string> fallback_app_name;

    bool locally_installed = true;
    // These OS shortcut fields can't be true if |locally_installed| is false.
    bool add_to_applications_menu = true;
    bool add_to_desktop = true;
    bool add_to_quick_launch_bar = true;
    bool run_on_os_login = false;

    // These have no effect outside of Chrome OS.
    bool add_to_search = true;
    bool add_to_management = true;
    bool is_disabled = false;

    bool bypass_service_worker_check = false;
    bool require_manifest = false;

    std::vector<std::string> additional_search_terms;

    base::Optional<std::string> launch_query_params;
    base::Optional<SystemAppType> system_app_type;

    bool oem_installed = false;
  };
  // Starts a background web app installation process for a given
  // |web_contents|.
  virtual void InstallWebAppWithParams(
      content::WebContents* web_contents,
      const InstallParams& install_params,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback) = 0;

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // This doesn't fetch a manifest and doesn't perform all required steps for
  // External installed apps: use |PendingAppManager::Install| instead.
  virtual void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback) = 0;

  virtual void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      const base::Optional<InstallParams>& install_params,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback) = 0;

  // For backward compatibility with ExtensionSyncService-based system:
  // Starts background installation or an update of a bookmark app from the sync
  // system. |web_application_info| contains received sync data. Icons will be
  // downloaded from the icon URLs provided in |web_application_info|.
  virtual void InstallBookmarkAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  // Reinstall an existing web app. If |redownload_app_icons| is true, will
  // redownload app icons and update them on disk. Otherwise, the icons in
  // |web_application_info.bitmap_icons| will be used and saved to disk.
  virtual void UpdateWebAppFromInfo(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool redownload_app_icons,
      OnceInstallCallback callback) = 0;

  explicit InstallManager(Profile* profile);
  virtual ~InstallManager();

  void SetSubsystems(AppRegistrar* registrar,
                     OsIntegrationManager* os_integration_manager,
                     InstallFinalizer* finalizer);

  // Loads |web_app_url| in a new WebContents and determines whether it has a
  // valid manifest. Calls |callback| with results.
  virtual void LoadWebAppAndCheckManifest(
      const GURL& web_app_url,
      webapps::WebappInstallSource install_source,
      WebAppManifestCheckCallback callback) = 0;

  void DisableBookmarkAppSyncInstallForTesting() {
    disable_bookmark_app_sync_install_for_testing_ = true;
  }
  void DisableWebAppSyncInstallForTesting() {
    disable_web_app_sync_install_for_testing_ = true;
  }
  virtual std::set<AppId> GetEnqueuedInstallAppIdsForTesting() = 0;

 protected:
  Profile* profile() { return profile_; }
  AppRegistrar* registrar() { return registrar_; }
  OsIntegrationManager* os_integration_manager() {
    return os_integration_manager_;
  }
  InstallFinalizer* finalizer() { return finalizer_; }

  bool disable_bookmark_app_sync_install_for_testing() const {
    return disable_bookmark_app_sync_install_for_testing_;
  }
  bool disable_web_app_sync_install_for_testing() const {
    return disable_web_app_sync_install_for_testing_;
  }

 private:
  Profile* const profile_;
  WebAppUrlLoader url_loader_;

  AppRegistrar* registrar_ = nullptr;
  OsIntegrationManager* os_integration_manager_ = nullptr;
  InstallFinalizer* finalizer_ = nullptr;

  bool disable_bookmark_app_sync_install_for_testing_ = false;
  bool disable_web_app_sync_install_for_testing_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
