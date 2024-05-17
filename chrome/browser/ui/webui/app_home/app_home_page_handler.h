// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX));

class ExtensionEnableFlow;

namespace content {
class WebUI;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionSystem;
class ExtensionUninstallDialog;
}  // namespace extensions

namespace web_app {
class WebAppProvider;
class AppLock;
}  // namespace web_app

namespace webapps {

class AppHomePageHandler
    : public app_home::mojom::PageHandler,
      public web_app::WebAppInstallManagerObserver,
      public extensions::ExtensionRegistryObserver,
      public extensions::ExtensionUninstallDialog::Delegate,
      public ExtensionEnableFlowDelegate,
      public web_app::WebAppRegistrarObserver {
 public:
  AppHomePageHandler(
      content::WebUI*,
      Profile* profile,
      mojo::PendingReceiver<app_home::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_home::mojom::Page> page);

  AppHomePageHandler(const AppHomePageHandler&) = delete;
  AppHomePageHandler& operator=(const AppHomePageHandler&) = delete;

  ~AppHomePageHandler() override;

  // web_app::WebAppInstallManagerObserver:
  // Listens to both `OnWebAppInstalled` and `OnWebAppInstalledWithOsHooks` as
  // some type of installs, e.g. sync install only trigger `OnWebAppInstalled`.
  // `OnWebAppInstalledWithOsHooks` also gets fired when an installed app gets
  // locally installed.
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // web_app::WebAppRegistrarObserver:
  void OnWebAppRunOnOsLoginModeChanged(
      const webapps::AppId& app_id,
      web_app::RunOnOsLoginMode run_on_os_login_mode) override;
  void OnWebAppUserDisplayModeChanged(
      const webapps::AppId& app_id,
      web_app::mojom::UserDisplayMode user_display_mode) override;
  void OnAppRegistrarDestroyed() override;

  // app_home::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;
  void GetDeprecationLinkString(
      GetDeprecationLinkStringCallback callback) override;
  void UninstallApp(const std::string& app_id) override;
  void ShowAppSettings(const std::string& app_id) override;
  void CreateAppShortcut(const std::string& app_id,
                         CreateAppShortcutCallback callback) override;
  void LaunchApp(const std::string& app_id,
                 app_home::mojom::ClickEventPtr click_event) override;
  void SetRunOnOsLoginMode(
      const std::string& app_id,
      web_app::RunOnOsLoginMode run_on_os_login_mode) override;
  void LaunchDeprecatedAppDialog() override;
  void InstallAppLocally(const std::string& app_id) override;
  void SetUserDisplayMode(
      const std::string& app_id,
      web_app::mojom::UserDisplayMode display_mode) override;

  app_home::mojom::AppInfoPtr GetApp(const webapps::AppId& app_id);

 private:
  Browser* GetCurrentBrowser();

  // Used to load the deprecated apps dialog if a chrome app is launched from
  // the command line.
  void LoadDeprecatedAppsDialogIfRequired();

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  extensions::ExtensionUninstallDialog* CreateExtensionUninstallDialog();

  // Prompts the user to re-enable the extension app for |extension_app_id|.
  void PromptToEnableExtensionApp(const std::string& extension_app_id);

  // Reset some instance flags we use to track the currently prompting app.
  void ResetExtensionDialogState();

  void ExtensionRemoved(const extensions::Extension* extension);

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override;

  void InstallOsHooks(const webapps::AppId& app_id, web_app::AppLock* lock);
  void LaunchAppInternal(const std::string& app_id,
                         extension_misc::AppLaunchBucket bucket,
                         app_home::mojom::ClickEventPtr click_event);
  void ShowWebAppSettings(const std::string& app_id);
  void ShowExtensionAppSettings(const extensions::Extension* extension);
  void CreateWebAppShortcut(const std::string& app_id, base::OnceClosure done);
  void CreateExtensionAppShortcut(const extensions::Extension* extension,
                                  base::OnceClosure done);
  // ExtensionEnableFlowDelegate:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  void UninstallWebApp(const std::string& web_app_id);
  void UninstallExtensionApp(const extensions::Extension* extension);
  void FillWebAppInfoList(std::vector<app_home::mojom::AppInfoPtr>* result);
  void FillExtensionInfoList(std::vector<app_home::mojom::AppInfoPtr>* result);
  app_home::mojom::AppInfoPtr CreateAppInfoPtrFromWebApp(
      const webapps::AppId& app_id);
  app_home::mojom::AppInfoPtr CreateAppInfoPtrFromExtension(
      const extensions::Extension* extension);

  raw_ptr<content::WebUI> web_ui_;

  raw_ptr<Profile> profile_;

  mojo::Receiver<app_home::mojom::PageHandler> receiver_;

  mojo::Remote<app_home::mojom::Page> page_;

  // The apps are represented in the web apps model, which outlives this class
  // since it's owned by |profile_|.
  const raw_ptr<web_app::WebAppProvider> web_app_provider_;

  // The apps are represented in the extensions model, which
  // outlives this class since it's owned by |profile_|.
  const raw_ref<extensions::ExtensionSystem> extension_system_;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      web_app_registrar_observation_{this};

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  std::unique_ptr<extensions::ExtensionUninstallDialog>
      extension_uninstall_dialog_;

  bool extension_dialog_prompting_ = false;

  // Used to show confirmation UI for enabling extensions.
  std::unique_ptr<ExtensionEnableFlow> extension_enable_flow_;
  // Set of deprecated app ids for showing on dialog.
  std::set<extensions::ExtensionId> deprecated_app_ids_;

  // Do not spam showing the dialog on every app install or any changes on the
  // page. Only show the dialog once the page loads when this class gets
  // constructed.
  bool has_maybe_loaded_deprecated_apps_dialog_ = false;

  // Used for passing callbacks.
  base::WeakPtrFactory<AppHomePageHandler> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_PAGE_HANDLER_H_
