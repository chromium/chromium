// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_H_

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class BrowserWindow;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace web_app {

class AppLock;
// WebAppUiManagerImpl can be used only in UI code.
class WebAppUiManagerImpl;

using UninstallScheduledCallback = base::OnceCallback<void(bool)>;
using UninstallCompleteCallback =
    base::OnceCallback<void(webapps::UninstallResultCode code)>;
using WebAppLaunchAcceptanceCallback =
    base::OnceCallback<void(bool allowed, bool remember_user_choice)>;

// Overrides the app identity update dialog's behavior for testing, allowing the
// test to auto-accept or auto-skip the dialog.
base::AutoReset<absl::optional<AppIdentityUpdate>>
SetIdentityUpdateDialogActionForTesting(
    absl::optional<AppIdentityUpdate> auto_accept_action);

absl::optional<AppIdentityUpdate> GetIdentityUpdateDialogActionForTesting();

class WebAppUiManagerObserver : public base::CheckedObserver {
 public:
  // Notifies on `content::WebContentsObserver::ReadyToCommitNavigation` when a
  // navigation is about to commit in a web app identified by `app_id`
  // (including navigations in sub frames).
  virtual void OnReadyToCommitNavigation(
      const webapps::AppId& app_id,
      content::NavigationHandle* navigation_handle) {}

  // Called when the WebAppUiManager is about to be destroyed.
  virtual void OnWebAppUiManagerDestroyed() {}
};

using LaunchWebAppCallback =
    base::OnceCallback<void(base::WeakPtr<Browser> browser,
                            base::WeakPtr<content::WebContents> web_contents,
                            apps::LaunchContainer container)>;

enum class LaunchWebAppWindowSetting {
  // The window container and disposition from the launch params are used,
  // despite the configuration of the web app.
  kUseLaunchParams,
  // The container and disposition of the launch are overridden with the
  // configuration of the web app, which include the user preference as well as
  // configuration in the web app's manifest.
  kOverrideWithWebAppConfig,
};

// A chrome/browser/ representation of the chrome/browser/ui/ UI manager to
// perform Web App UI operations or listen to Web App UI events, including
// events from WebAppTabHelpers.
class WebAppUiManager {
 public:
  static std::unique_ptr<WebAppUiManager> Create(Profile* profile);

  // The returned params are populated except for the disposition and container,
  // which is expected to be populated later when using `LaunchWebApp`
  // with `kOverrideWithWebAppConfig`.
  static apps::AppLaunchParams CreateAppLaunchParamsWithoutWindowConfig(
      const webapps::AppId& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      const absl::optional<GURL>& url_handler_launch_url,
      const absl::optional<GURL>& protocol_handler_launch_url,
      const absl::optional<GURL>& file_launch_url,
      const std::vector<base::FilePath>& launch_files);

  WebAppUiManager();
  virtual ~WebAppUiManager();

  base::WeakPtr<WebAppUiManager> GetWeakPtr();

  virtual void Start() = 0;
  virtual void Shutdown() = 0;

  // A safe downcast.
  virtual WebAppUiManagerImpl* AsImpl() = 0;

  virtual size_t GetNumWindowsForApp(const webapps::AppId& app_id) = 0;

  // Close app windows. Does not affect tabs in a non-app browser.
  virtual void CloseAppWindows(const webapps::AppId& app_id) = 0;

  virtual void NotifyOnAllAppWindowsClosed(const webapps::AppId& app_id,
                                           base::OnceClosure callback) = 0;

  void AddObserver(WebAppUiManagerObserver* observer);
  void RemoveObserver(WebAppUiManagerObserver* observer);
  void NotifyReadyToCommitNavigation(
      const webapps::AppId& app_id,
      content::NavigationHandle* navigation_handle);

  virtual bool CanAddAppToQuickLaunchBar() const = 0;
  virtual void AddAppToQuickLaunchBar(const webapps::AppId& app_id) = 0;
  virtual bool IsAppInQuickLaunchBar(const webapps::AppId& app_id) const = 0;

  // Returns whether |web_contents| is in a web app window or popup window
  // created from a web app window.
  virtual bool IsInAppWindow(content::WebContents* web_contents) const = 0;
  virtual const webapps::AppId* GetAppIdForWindow(
      content::WebContents* web_contents) const = 0;
  virtual void NotifyOnAssociatedAppChanged(
      content::WebContents* web_contents,
      const absl::optional<webapps::AppId>& previous_app_id,
      const absl::optional<webapps::AppId>& new_app_id) const = 0;

  virtual bool CanReparentAppTabToWindow(const webapps::AppId& app_id,
                                         bool shortcut_created) const = 0;
  virtual void ReparentAppTabToWindow(content::WebContents* contents,
                                      const webapps::AppId& app_id,
                                      bool shortcut_created) = 0;

  // Shows the pre-launch dialog for a file handling web app launch. The user
  // can allow or block the launch.
  virtual void ShowWebAppFileLaunchDialog(
      const std::vector<base::FilePath>& file_paths,
      const webapps::AppId& app_id,
      WebAppLaunchAcceptanceCallback launch_callback) = 0;

  virtual void ShowWebAppIdentityUpdateDialog(
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      content::WebContents* web_contents,
      AppIdentityDialogCallback callback) = 0;

  // Show the settings UI for the given app.
  virtual void ShowWebAppSettings(const webapps::AppId& app_id) = 0;

  // This launches the web app in the appropriate configuration, the behavior of
  // which depends on the given configuration here and the configuration of the
  // web app. E.g. attaching file handles to the launch queue, focusing existing
  // windows if configured by the launch handlers, etc. See
  // `web_app::LaunchWebApp` and `WebAppLaunchProcess` for more info.
  // If the app_id is invalid, an empty browser window is opened.
  virtual void WaitForFirstRunAndLaunchWebApp(
      apps::AppLaunchParams params,
      LaunchWebAppWindowSetting launch_setting,
      Profile& profile,
      LaunchWebAppCallback callback,
      AppLock& lock) = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Migrates launcher state, such as parent folder id, position in App Launcher
  // and pin position on the shelf from one app to another app.
  // Avoids migrating if the to_app_id is already pinned.
  virtual void MigrateLauncherState(const webapps::AppId& from_app_id,
                                    const webapps::AppId& to_app_id,
                                    base::OnceClosure callback) = 0;

  // Displays a notification for web apps launched on login via the RunOnOsLogin
  // feature on the provided |profile|.
  virtual void DisplayRunOnOsLoginNotification(
      const std::vector<std::string>& app_names,
      base::WeakPtr<Profile> profile) = 0;
#endif

  // Creates a new Browser tab on the "about:blank" URL. Creates a new browser
  // if there isn't one that is already open.
  virtual content::WebContents* CreateNewTab() = 0;

  // Check if a tab is the currently active tab in the browser.
  virtual bool IsWebContentsActiveTabInBrowser(
      content::WebContents* web_contents) = 0;

  // Triggers the web app install dialog on the specified |web_contents| if
  // there is an installable web app. This will show the dialog even if the app
  // is already installed.
  virtual void TriggerInstallDialog(content::WebContents* web_contents) = 0;

  // The uninstall dialog will be modal to |parent_window|, or a non-modal if
  // |parent_window| is nullptr. Use this API if a Browser window needs to be
  // passed in along with an UninstallCompleteCallback.
  virtual void PresentUserUninstallDialog(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      BrowserWindow* parent_window,
      UninstallCompleteCallback callback) = 0;

  // Use this API if a gfx::NativeWindow needs to be passed in along with an
  // UninstallCompleteCallback.
  virtual void PresentUserUninstallDialog(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback) = 0;

  // Use this API if a gfx::NativeWindow needs to be passed in along with a
  // UninstallCompleteCallback and an UninstallScheduledCallback.
  virtual void PresentUserUninstallDialog(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback,
      UninstallScheduledCallback scheduled_callback) = 0;

  // Launches the Isolated Web App installer for a bundle with the given path.
  virtual void LaunchIsolatedWebAppInstaller(
      const base::FilePath& bundle_path) = 0;

  // Creates the EnableSupportedLinksInfobar in an app window when the app is
  // launched via link capturing from a link.
  virtual void MaybeCreateEnableSupportedLinksInfobar(
      content::WebContents* web_contents,
      const std::string& launch_name) = 0;

 private:
  base::ObserverList<WebAppUiManagerObserver, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<WebAppUiManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_H_
