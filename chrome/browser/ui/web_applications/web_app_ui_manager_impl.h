// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class BrowserWindow;
class Profile;
class SkBitmap;

namespace apps {
struct AppLaunchParams;
}

namespace base {
class FilePath;
}

namespace views {
class NativeWindowTracker;
}  // namespace views

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class IsolatedWebAppInstallerCoordinator;
class WithAppResources;

// Implementation of WebAppUiManager that depends upon //c/b/ui.
// Allows //c/b/web_applications code to call into //c/b/ui without directly
// depending on UI.
class WebAppUiManagerImpl : public BrowserListObserver,
                            public WebAppUiManager,
                            public TabStripModelObserver {
 public:
  explicit WebAppUiManagerImpl(Profile* profile);
  WebAppUiManagerImpl(const WebAppUiManagerImpl&) = delete;
  WebAppUiManagerImpl& operator=(const WebAppUiManagerImpl&) = delete;
  ~WebAppUiManagerImpl() override;

  void Start() override;
  void Shutdown() override;

  // WebAppUiManager:
  WebAppUiManagerImpl* AsImpl() override;
  size_t GetNumWindowsForApp(const webapps::AppId& app_id) override;
  void CloseAppWindows(const webapps::AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const webapps::AppId& app_id,
                                   base::OnceClosure callback) override;
  bool CanAddAppToQuickLaunchBar() const override;
  void AddAppToQuickLaunchBar(const webapps::AppId& app_id) override;
  bool IsAppInQuickLaunchBar(const webapps::AppId& app_id) const override;
  bool IsInAppWindow(content::WebContents* web_contents) const override;
  const webapps::AppId* GetAppIdForWindow(
      const content::WebContents* web_contents) const override;
  void NotifyOnAssociatedAppChanged(
      content::WebContents* web_contents,
      const std::optional<webapps::AppId>& previous_app_id,
      const std::optional<webapps::AppId>& new_app_id) const override;
  bool CanReparentAppTabToWindow(const webapps::AppId& app_id,
                                 bool shortcut_created) const override;
  Browser* ReparentAppTabToWindow(content::WebContents* contents,
                                  const webapps::AppId& app_id,
                                  bool shortcut_created) override;
  Browser* ReparentAppTabToWindow(
      content::WebContents* contents,
      const webapps::AppId& app_id,
      base::OnceCallback<void(content::WebContents*)> completion_callback)
      override;
  void ShowWebAppFileLaunchDialog(
      const std::vector<base::FilePath>& file_paths,
      const webapps::AppId& app_id,
      WebAppLaunchAcceptanceCallback launch_callback) override;
  void ShowWebAppIdentityUpdateDialog(
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      content::WebContents* web_contents,
      web_app::AppIdentityDialogCallback callback) override;
  void ShowWebAppSettings(const webapps::AppId& app_id) override;
  void LaunchWebApp(apps::AppLaunchParams params,
                    LaunchWebAppWindowSetting launch_setting,
                    Profile& profile,
                    LaunchWebAppDebugValueCallback callback,
                    WithAppResources& lock) override;
  void WaitForFirstRunService(
      Profile& profile,
      FirstRunServiceCompletedCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS)
  void MigrateLauncherState(const webapps::AppId& from_app_id,
                            const webapps::AppId& to_app_id,
                            base::OnceClosure callback) override;

  void DisplayRunOnOsLoginNotification(
      const base::flat_map<webapps::AppId, RoolNotificationBehavior>& apps,
      base::WeakPtr<Profile> profile) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  void NotifyAppRelaunchState(const webapps::AppId& placeholder_app_id,
                              const webapps::AppId& final_app_id,
                              const std::u16string& final_app_name,
                              base::WeakPtr<Profile> profile,
                              AppRelaunchState relaunch_state) override;

  content::WebContents* CreateNewTab() override;
  bool IsWebContentsActiveTabInBrowser(
      content::WebContents* web_contents) override;
  void TriggerInstallDialog(content::WebContents* web_contents) override;

  void PresentUserUninstallDialog(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      BrowserWindow* parent_window,
      UninstallCompleteCallback callback) override;

  void PresentUserUninstallDialog(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback) override;

  void PresentUserUninstallDialog(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback,
      UninstallScheduledCallback scheduled_callback) override;

  void LaunchOrFocusIsolatedWebAppInstaller(
      const base::FilePath& bundle_path) override;

  void MaybeCreateEnableSupportedLinksInfobar(
      content::WebContents* web_contents,
      const std::string& launch_name) override;

  void MaybeShowIPHPromoForAppsLaunchedViaLinkCapturing(
      Browser* browser,
      Profile* profile,
      const std::string& app_id) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
#if BUILDFLAG(IS_CHROMEOS)
  void OnBrowserCloseCancelled(Browser* browser,
                               BrowserClosingStatus reason) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  void OnBrowserRemoved(Browser* browser) override;

#if BUILDFLAG(IS_CHROMEOS)
  // TabStripModelObserver:
  void TabCloseCancelled(const content::WebContents* contents) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  // Attempts to uninstall the given web app id. Meant to be used with OS-level
  // uninstallation support/hooks.
  void UninstallWebAppFromStartupSwitch(const webapps::AppId& app_id);
#endif

 private:
  // Returns true if Browser is for an installed App.
  bool IsBrowserForInstalledApp(Browser* browser);

  // Returns webapps::AppId of the Browser's installed App,
  // |IsBrowserForInstalledApp| must be true.
  webapps::AppId GetAppIdForBrowser(Browser* browser);

  void OnExtensionSystemReady();

  void OnIconsReadForUninstall(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      std::unique_ptr<views::NativeWindowTracker> parent_window_tracker,
      UninstallCompleteCallback complete_callback,
      UninstallScheduledCallback uninstall_scheduled_callback,
      std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  void OnIsolatedWebAppInstallerClosed(base::FilePath bundle_path);

  void ScheduleUninstallIfUserRequested(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCompleteCallback complete_callback,
      UninstallScheduledCallback uninstall_scheduled_callback,
      web_app::UninstallUserOptions uninstall_options);

  void OnUninstallCancelled(
      UninstallCompleteCallback complete_callback,
      UninstallScheduledCallback uninstall_scheduled_callback);

  void ClearWebAppSiteDataIfNeeded(
      const GURL app_start_url,
      UninstallCompleteCallback uninstall_complete_callback,
      webapps::UninstallResultCode uninstall_code);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void ShowIPHPromoForAppsLaunchedViaLinkCapturing(const Browser* browser,
                                                   const webapps::AppId& app_id,
                                                   bool is_activated);

  void OnIPHPromoResponseForLinkCapturing(const Browser* browser,
                                          const webapps::AppId& app_id);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  const raw_ptr<Profile> profile_;
  std::map<webapps::AppId, std::vector<base::OnceClosure>>
      windows_closed_requests_map_;
  std::map<webapps::AppId, size_t> num_windows_for_apps_map_;
  std::map<base::FilePath, raw_ptr<IsolatedWebAppInstallerCoordinator>>
      active_installers_;
  bool started_ = false;

  base::WeakPtrFactory<WebAppUiManagerImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
