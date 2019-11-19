// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace web_app {

enum class ExternalInstallSource;
enum class InstallResultCode;
class AppRegistrar;
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
    WebappInstallSource install_source = WebappInstallSource::COUNT;
    bool locally_installed = true;
  };

  // Write the WebApp data to disk and register the app.
  virtual void FinalizeInstall(const WebApplicationInfo& web_app_info,
                               const FinalizeOptions& options,
                               InstallFinalizedCallback callback) = 0;

  // For the new USS-based system only. Generate missing sync placeholder data
  // and icons using |sync_data| fields.
  virtual void FinalizeFallbackInstallAfterSync(
      const AppId& app_id,
      InstallFinalizedCallback callback) = 0;
  // Delete app data from disk (icon .png files). |app_id| must be unregistered.
  virtual void FinalizeUninstallAfterSync(const AppId& app_id,
                                          UninstallWebAppCallback callback) = 0;

  // Write the new WebApp data to disk and update the app.
  virtual void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                              InstallFinalizedCallback callback) = 0;

  // Removes the external app for |app_url| from disk and registrar. Fails if
  // there is no installed external app for |app_url|.
  virtual void UninstallExternalWebApp(
      const GURL& app_url,
      ExternalInstallSource external_install_source,
      UninstallWebAppCallback) = 0;

  virtual bool CanUserUninstallFromSync(const AppId& app_id) const = 0;
  virtual void UninstallWebAppFromSyncByUser(const AppId& app_id,
                                             UninstallWebAppCallback) = 0;

  // |virtual| for testing.
  virtual bool CanAddAppToQuickLaunchBar() const;
  virtual void AddAppToQuickLaunchBar(const AppId& app_id);

  // |virtual| for testing.
  virtual bool CanReparentTab(const AppId& app_id, bool shortcut_created) const;
  virtual void ReparentTab(const AppId& app_id,
                           bool shortcut_created,
                           content::WebContents* web_contents);

  virtual bool CanRevealAppShim() const = 0;
  virtual void RevealAppShim(const AppId& app_id) = 0;

  void SetSubsystems(AppRegistrar* registrar, WebAppUiManager* ui_manager);

  virtual ~InstallFinalizer() = default;

 protected:
  AppRegistrar& registrar() const { return *registrar_; }
  WebAppUiManager& ui_manager() const { return *ui_manager_; }

 private:
  AppRegistrar* registrar_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
