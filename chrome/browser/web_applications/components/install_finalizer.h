// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

struct WebApplicationInfo;
class GURL;

namespace content {
class WebContents;
}

namespace web_app {

enum class ExternalInstallSource;
enum class InstallResultCode;
class AppRegistrar;
class AppRegistryController;
class WebAppUiManager;

// An abstract finalizer for the installation process, represents the last step.
// Takes WebApplicationInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class InstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;
  using UninstallWebAppCallback = base::OnceCallback<void(bool uninstalled)>;

  struct FinalizeOptions {
    FinalizeOptions();
    ~FinalizeOptions();
    FinalizeOptions(const FinalizeOptions&);

    WebappInstallSource install_source = WebappInstallSource::COUNT;
    bool locally_installed = true;

    base::Optional<WebAppChromeOsData> chromeos_data;
  };

  // Write the WebApp data to disk and register the app.
  virtual void FinalizeInstall(const WebApplicationInfo& web_app_info,
                               const FinalizeOptions& options,
                               InstallFinalizedCallback callback) = 0;

  // Delete app data from disk (icon .png files). |app_id| must be unregistered.
  virtual void FinalizeUninstallAfterSync(const AppId& app_id,
                                          UninstallWebAppCallback callback) = 0;

  // Write the new WebApp data to disk and update the app.
  virtual void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                              InstallFinalizedCallback callback) = 0;

  // Removes |external_install_source| from |app_id|. If no more interested
  // sources left, deletes the app from disk and registrar.
  virtual void UninstallExternalWebApp(
      const AppId& app_id,
      ExternalInstallSource external_install_source,
      UninstallWebAppCallback callback) = 0;

  // Removes the external app for |app_url| from disk and registrar. Fails if
  // there is no installed external app for |app_url|. Virtual for testing.
  virtual void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      ExternalInstallSource external_install_source,
      UninstallWebAppCallback callback);

  virtual bool CanUserUninstallFromSync(const AppId& app_id) const = 0;
  virtual void UninstallWebAppFromSyncByUser(const AppId& app_id,
                                             UninstallWebAppCallback) = 0;

  virtual bool CanUserUninstallExternalApp(const AppId& app_id) const = 0;
  // If external app is synced, uninstalls it from sync and from all devices.
  virtual void UninstallExternalAppByUser(const AppId& app_id,
                                          UninstallWebAppCallback callback) = 0;
  // Returns true if the app with |app_id| was previously uninstalled by the
  // user. For example, if a user uninstalls a default app ('default apps' are
  // considered external apps), then this will return true.
  virtual bool WasExternalAppUninstalledByUser(const AppId& app_id) const = 0;

  // |virtual| for testing.
  virtual bool CanReparentTab(const AppId& app_id, bool shortcut_created) const;
  virtual void ReparentTab(const AppId& app_id,
                           bool shortcut_created,
                           content::WebContents* web_contents);

  virtual void RemoveLegacyInstallFinalizerForTesting() {}
  virtual InstallFinalizer* legacy_finalizer_for_testing();

  virtual void Start() {}
  virtual void Shutdown() {}

  void SetSubsystems(AppRegistrar* registrar,
                     WebAppUiManager* ui_manager,
                     AppRegistryController* registry_controller);

  virtual ~InstallFinalizer() = default;

 protected:
  bool is_legacy_finalizer() const { return registrar_ == nullptr; }
  AppRegistrar& registrar() const;

  WebAppUiManager& ui_manager() const { return *ui_manager_; }
  AppRegistryController& registry_controller() { return *registry_controller_; }

 private:
  // If these pointers are nullptr then this is legacy install finalizer
  // operating in standalone mode.
  AppRegistrar* registrar_ = nullptr;
  AppRegistryController* registry_controller_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
