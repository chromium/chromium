// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_UI_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_UI_MANAGER_H_

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class FakeWebAppUiManager : public WebAppUiManager {
 public:
  FakeWebAppUiManager();
  FakeWebAppUiManager(const FakeWebAppUiManager&) = delete;
  FakeWebAppUiManager& operator=(const FakeWebAppUiManager&) = delete;
  ~FakeWebAppUiManager() override;

  using OnLaunchWebAppCallback =
      base::RepeatingCallback<void(apps::AppLaunchParams params,
                                   LaunchWebAppWindowSetting launch_setting)>;

  void Start() override;
  void Shutdown() override;

  void SetNumWindowsForApp(const AppId& app_id, size_t num_windows_for_app);
  void SetOnNotifyOnAllAppWindowsClosedCallback(
      base::RepeatingCallback<void(AppId)> callback);
  int num_reparent_tab_calls() const { return num_reparent_tab_calls_; }

  void SetOnLaunchWebAppCallback(OnLaunchWebAppCallback callback);

  // WebAppUiManager:
  WebAppUiManagerImpl* AsImpl() override;
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;
  bool CanAddAppToQuickLaunchBar() const override;
  void AddAppToQuickLaunchBar(const AppId& app_id) override;
  bool IsAppInQuickLaunchBar(const AppId& app_id) const override;
  bool IsInAppWindow(content::WebContents* web_contents,
                     const AppId* app_id) const override;
  void NotifyOnAssociatedAppChanged(
      content::WebContents* web_contents,
      const absl::optional<AppId>& previous_app_id,
      const absl::optional<AppId>& new_app_id) const override {}
  bool CanReparentAppTabToWindow(const AppId& app_id,
                                 bool shortcut_created) const override;
  void ReparentAppTabToWindow(content::WebContents* contents,
                              const AppId& app_id,
                              bool shortcut_created) override;
  void ShowWebAppIdentityUpdateDialog(
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      content::WebContents* web_contents,
      AppIdentityDialogCallback callback) override;

  base::Value LaunchWebApp(apps::AppLaunchParams params,
                           LaunchWebAppWindowSetting launch_setting,
                           Profile& profile,
                           LaunchWebAppCallback callback,
                           AppLock& lock) override;
#if BUILDFLAG(IS_CHROMEOS)
  void MigrateLauncherState(const AppId& from_app_id,
                            const AppId& to_app_id,
                            base::OnceClosure callback) override;

  void DisplayRunOnOsLoginNotification(
      const std::vector<std::string>& app_names,
      base::WeakPtr<Profile> profile) override;
#endif
  content::WebContents* CreateNewTab() override;
  void TriggerInstallDialog(content::WebContents* web_contents) override;

  void PresentUserUninstallDialog(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      BrowserWindow* parent_window,
      UninstallCompleteCallback callback) override;

  void PresentUserUninstallDialog(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback) override;

  void PresentUserUninstallDialog(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback,
      UninstallScheduledCallback scheduled_callback) override;

 private:
  base::flat_map<AppId, size_t> app_id_to_num_windows_map_;
  // Closures waiting to be called when all windows for a given `AppId` are
  // closed.
  base::flat_map<AppId, std::vector<base::OnceClosure>>
      windows_closed_requests_map_;

  // Callback that is triggered after `NotifyOnAllAppWindowsClosed` is called.
  base::RepeatingCallback<void(AppId)>
      notify_on_all_app_windows_closed_callback_ = base::DoNothing();

  int num_reparent_tab_calls_ = 0;
  OnLaunchWebAppCallback on_launch_web_app_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_UI_MANAGER_H_
