// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_UI_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_UI_MANAGER_H_

#include <map>

#include "base/functional/callback_forward.h"
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

  void SetSubsystems(WebAppSyncBridge* sync_bridge,
                     OsIntegrationManager* os_integration_manager) override;
  void Start() override;
  void Shutdown() override;

  void SetNumWindowsForApp(const AppId& app_id, size_t num_windows_for_app);
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
  void MaybeTransferAppAttributes(const AppId& from_extension_or_app,
                                  const AppId& to_app) override;
  content::WebContents* CreateNewTab() override;
  void TriggerInstallDialog(content::WebContents* web_contents) override;

 private:
  std::map<AppId, size_t> app_id_to_num_windows_map_;
  int num_reparent_tab_calls_ = 0;
  OnLaunchWebAppCallback on_launch_web_app_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_UI_MANAGER_H_
