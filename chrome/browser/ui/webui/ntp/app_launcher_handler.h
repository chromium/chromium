// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ExtensionEnableFlow;
class PrefChangeRegistrar;
class Profile;

namespace extensions {
class ExtensionService;
}  // namespace extensions

namespace favicon_base {
struct FaviconImageResult;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {
class WebAppProvider;
}  // namespace web_app

// The handler for Javascript messages related to the "apps" view.
// TODO(https://crbug.com/1295802): This class should listen to changes from
// other sources (such as other app pages or web app settings page).
class AppLauncherHandler
    : public content::WebUIMessageHandler,
      public extensions::ExtensionUninstallDialog::Delegate,
      public ExtensionEnableFlowDelegate,
      public extensions::InstallObserver,
      public web_app::WebAppRegistrarObserver,
      public web_app::WebAppInstallManagerObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  AppLauncherHandler(extensions::ExtensionService* extension_service,
                     web_app::WebAppProvider* web_app_provider);

  AppLauncherHandler(const AppLauncherHandler&) = delete;
  AppLauncherHandler& operator=(const AppLauncherHandler&) = delete;

  ~AppLauncherHandler() override;

  base::Value::Dict CreateWebAppInfo(const web_app::AppId& app_id);

  base::Value::Dict CreateExtensionInfo(const extensions::Extension* extension);

  // Registers values (strings etc.) for the page.
  static void RegisterLoadTimeData(Profile* profile,
                                   content::WebUIDataSource* source);

  // Register per-profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // extensions::InstallObserver
  void OnAppsReordered(
      content::BrowserContext* context,
      const absl::optional<std::string>& extension_id) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // web_app::OnWebAppInstallManagerObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppUninstalled(
      const web_app::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

  // web_app::WebAppRegistrarObserver:
  void OnWebAppInstallTimeChanged(const web_app::AppId& app_id,
                                  const base::Time& time) override;
  void OnAppRegistrarDestroyed() override;
  void OnWebAppRunOnOsLoginModeChanged(
      const web_app::AppId& app_id,
      web_app::RunOnOsLoginMode run_on_os_login_mode) override;
  void OnWebAppSettingsPolicyChanged() override;

  // Populate the given dictionary with all installed app info.
  void FillAppDictionary(base::Value::Dict* value);

  // Create a dictionary value for the given extension.
  base::Value::Dict GetExtensionInfo(const extensions::Extension* extension);

  // Create a dictionary value for the given web app.
  base::Value::Dict GetWebAppInfo(const web_app::AppId& app_id);

  // Handles the "launchApp" message with unused |args|.
  void HandleGetApps(const base::Value::List& args);

  // Handles the "launchApp" message with |args| containing [extension_id,
  // source] with optional [url, disposition], |disposition| defaulting to
  // CURRENT_TAB.
  void HandleLaunchApp(const base::Value::List& args);

  void LaunchApp(std::string extension_id,
                 extension_misc::AppLaunchBucket launch_bucket,
                 const std::string& source_value,
                 WindowOpenDisposition disposition,
                 bool force_launch_deprecated_apps);

  // Handles the "setLaunchType" message with args containing [extension_id,
  // launch_type].
  void HandleSetLaunchType(const base::Value::List& args);

  // Handles the "uninstallApp" message with |args| containing [extension_id]
  // and an optional bool to not confirm the uninstall when true, defaults to
  // false.
  void HandleUninstallApp(const base::Value::List& args);

  // Handles the "createAppShortcut" message with |args| containing
  // [extension_id].
  void HandleCreateAppShortcut(base::OnceClosure done,
                               const base::Value::List& args);

  // Handles the "installAppLocally" message with |args| containing
  // [extension_id].
  void HandleInstallAppLocally(const base::Value::List& args);

  // Handles the "showAppInfo" message with |args| containing [extension_id].
  void HandleShowAppInfo(const base::Value::List& args);

  // Handles the "reorderApps" message with |args| containing [dragged_app_id,
  // app_order].
  void HandleReorderApps(const base::Value::List& args);

  // Handles the "setPageIndex" message with |args| containing [extension_id,
  // page_index].
  void HandleSetPageIndex(const base::Value::List& args);

  // Handles "saveAppPageName" message with |args| containing [name,
  // page_index].
  void HandleSaveAppPageName(const base::Value::List& args);

  // Handles "generateAppForLink" message with |args| containing [url, title,
  // page_index].
  void HandleGenerateAppForLink(const base::Value::List& args);

  // Handles "pageSelected" message with |args| containing [page_index].
  void HandlePageSelected(const base::Value::List& args);

  // Handles "runOnOsLogin" message with |args| containing [app_id, mode]
  void HandleRunOnOsLogin(const base::Value::List& args);

  // Handles "deprecatedDialogLinkClicked" message with no |args|
  void HandleLaunchDeprecatedAppDialog(const base::Value::List& args);

 private:
  FRIEND_TEST_ALL_PREFIXES(AppLauncherHandlerTest,
                           HandleClosedWhileUninstallingExtension);

  struct AppInstallInfo {
    AppInstallInfo();
    ~AppInstallInfo();

    std::u16string title;
    GURL app_url;
    syncer::StringOrdinal page_ordinal;
  };

  // Reset some instance flags we use to track the currently uninstalling app.
  void CleanupAfterUninstall();

  // Prompts the user to re-enable the app for |extension_id|.
  void PromptToEnableApp(const std::string& extension_id);

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override;

  // ExtensionEnableFlowDelegate:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  extensions::ExtensionUninstallDialog* CreateExtensionUninstallDialog();

  // Continuation for installing a bookmark app after favicon lookup.
  void OnFaviconForAppInstallFromLink(
      std::unique_ptr<AppInstallInfo> install_info,
      const favicon_base::FaviconImageResult& image_result);

  void OnExtensionPreferenceChanged();

  // Called when an extension is removed (unloaded or uninstalled). Updates the
  // UI.
  void ExtensionRemoved(const extensions::Extension* extension,
                        bool is_uninstall);

  // True if the extension should be displayed.
  bool ShouldShow(const extensions::Extension* extension);

  // The apps are represented in the extensions model, which
  // outlives us since it's owned by our containing profile.
  const raw_ptr<extensions::ExtensionService> extension_service_;

  // The apps are represented in the web apps model, which outlives us since
  // it's owned by our containing profile.
  const raw_ptr<web_app::WebAppProvider> web_app_provider_;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      web_apps_observation_{this};

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::ScopedObservation<extensions::InstallTracker,
                          extensions::InstallObserver>
      install_tracker_observation_{this};

  // Monitor extension preference changes so that the Web UI can be notified.
  PrefChangeRegistrar extension_pref_change_registrar_;

  // Monitor the local state pref to control the app launcher promo.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // Used to show confirmation UI for uninstalling extensions in incognito mode.
  std::unique_ptr<extensions::ExtensionUninstallDialog>
      extension_uninstall_dialog_;

  // Used to show confirmation UI for enabling extensions.
  std::unique_ptr<ExtensionEnableFlow> extension_enable_flow_;

  // The ids of apps to show on the NTP.
  std::set<std::string> visible_apps_;

  // Set of deprecated app ids for showing on dialog.
  std::set<extensions::ExtensionId> deprecated_app_ids_;

  // The id of the extension we are prompting the user about (either enable or
  // uninstall).
  extensions::ExtensionId extension_id_prompting_;

  // When true, we ignore changes to the underlying data rather than immediately
  // refreshing. This is useful when making many batch updates to avoid flicker.
  bool ignore_changes_;

  // When populated, we have attempted to install a bookmark app, and are still
  // waiting to hear about success or failure from the extensions system.
  absl::optional<syncer::StringOrdinal>
      attempting_web_app_install_page_ordinal_;

  // True if we have executed HandleGetApps() at least once.
  bool has_loaded_apps_;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Used for passing callbacks.
  base::WeakPtrFactory<AppLauncherHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
