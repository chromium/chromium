// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"

enum class WebappInstallSource;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

class Profile;

namespace web_app {

enum class InstallResultCode;
class InstallFinalizer;
class AppRegistrar;
class AppShortcutManager;

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
  // Callback with the result of an installability check.
  // |web_contents| owns the WebContents that was used to check installability.
  // |app_id| will be present iff already installed.
  using WebAppInstallabilityCheckCallback = base::OnceCallback<void(
      std::unique_ptr<content::WebContents> web_contents,
      InstallableCheckResult result,
      base::Optional<AppId> app_id)>;

  // Returns true if a web app can be installed for a given |web_contents|.
  virtual bool CanInstallWebApp(content::WebContents* web_contents) = 0;

  // Checks a WebApp installability, retrieves manifest and icons and
  // than performs the actual installation.
  virtual void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      WebappInstallSource install_source,
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
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) = 0;

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // InstallManager doesn't fetch a manifest.
  virtual void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      WebappInstallSource install_source,
      OnceInstallCallback callback) = 0;

  // These params are a subset of ExternalInstallOptions.
  struct InstallParams {
    DisplayMode user_display_mode = DisplayMode::kUndefined;

    // URL to be used as start_url if manifest is unavailable.
    GURL fallback_start_url;

    bool add_to_applications_menu = true;
    bool add_to_desktop = true;
    bool add_to_quick_launch_bar = true;

    bool bypass_service_worker_check = false;
    bool require_manifest = false;
  };
  // Starts a background web app installation process for a given
  // |web_contents|.
  virtual void InstallWebAppWithParams(content::WebContents* web_contents,
                                       const InstallParams& install_params,
                                       WebappInstallSource install_source,
                                       OnceInstallCallback callback) = 0;

  // For the old ExtensionSyncService-based system only:
  // Starts background installation or an update of a web app from the sync
  // system. |web_application_info| contains received sync data. Icons will be
  // downloaded from the icon URLs provided in |web_application_info|.
  virtual void InstallWebAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  // Reinstall an existing web app, will redownload icons and update them on
  // disk.
  virtual void UpdateWebAppFromInfo(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  virtual void Shutdown() = 0;

  explicit InstallManager(Profile* profile);
  virtual ~InstallManager();

  void SetSubsystems(AppRegistrar* registrar,
                     AppShortcutManager* shortcut_manager,
                     InstallFinalizer* finalizer);

  // Loads |web_app_url| in a new WebContents and determines whether it is
  // installable. Calls |callback| with results.
  virtual void LoadWebAppAndCheckInstallability(
      const GURL& web_app_url,
      WebappInstallSource install_source,
      WebAppInstallabilityCheckCallback callback) = 0;

 protected:
  Profile* profile() { return profile_; }
  AppRegistrar* registrar() { return registrar_; }
  AppShortcutManager* shortcut_manager() { return shortcut_manager_; }
  InstallFinalizer* finalizer() { return finalizer_; }

 private:
  Profile* const profile_;
  WebAppUrlLoader url_loader_;

  AppRegistrar* registrar_ = nullptr;
  AppShortcutManager* shortcut_manager_ = nullptr;
  InstallFinalizer* finalizer_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
