// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

FakeWebAppUiManager::FakeWebAppUiManager() = default;

FakeWebAppUiManager::~FakeWebAppUiManager() = default;

void FakeWebAppUiManager::Start() {}

void FakeWebAppUiManager::Shutdown() {}

void FakeWebAppUiManager::SetNumWindowsForApp(const AppId& app_id,
                                              size_t num_windows_for_app) {
  app_id_to_num_windows_map_[app_id] = num_windows_for_app;

  if (num_windows_for_app != 0) {
    return;
  }

  auto it = windows_closed_requests_map_.find(app_id);
  if (it == windows_closed_requests_map_.end()) {
    return;
  }
  for (auto& callback : it->second) {
    std::move(callback).Run();
  }

  windows_closed_requests_map_.erase(it);
}

void FakeWebAppUiManager::SetOnNotifyOnAllAppWindowsClosedCallback(
    base::RepeatingCallback<void(AppId)> callback) {
  notify_on_all_app_windows_closed_callback_ = std::move(callback);
}

void FakeWebAppUiManager::SetOnLaunchWebAppCallback(
    OnLaunchWebAppCallback callback) {
  on_launch_web_app_callback_ = std::move(callback);
}

WebAppUiManagerImpl* FakeWebAppUiManager::AsImpl() {
  return nullptr;
}

size_t FakeWebAppUiManager::GetNumWindowsForApp(const AppId& app_id) {
  if (!app_id_to_num_windows_map_.contains(app_id)) {
    return 0;
  }
  return app_id_to_num_windows_map_[app_id];
}

void FakeWebAppUiManager::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  notify_on_all_app_windows_closed_callback_.Run(app_id);

  if (GetNumWindowsForApp(app_id) == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

bool FakeWebAppUiManager::CanAddAppToQuickLaunchBar() const {
  return false;
}

void FakeWebAppUiManager::AddAppToQuickLaunchBar(const AppId& app_id) {}

bool FakeWebAppUiManager::IsAppInQuickLaunchBar(const AppId& app_id) const {
  return false;
}

bool FakeWebAppUiManager::IsInAppWindow(content::WebContents* web_contents,
                                        const AppId* app_id) const {
  return false;
}

bool FakeWebAppUiManager::CanReparentAppTabToWindow(
    const AppId& app_id,
    bool shortcut_created) const {
  return true;
}

void FakeWebAppUiManager::ReparentAppTabToWindow(content::WebContents* contents,
                                                 const AppId& app_id,
                                                 bool shortcut_created) {
  ++num_reparent_tab_calls_;
}

void FakeWebAppUiManager::ShowWebAppIdentityUpdateDialog(
    const std::string& app_id,
    bool title_change,
    bool icon_change,
    const std::u16string& old_title,
    const std::u16string& new_title,
    const SkBitmap& old_icon,
    const SkBitmap& new_icon,
    content::WebContents* web_contents,
    AppIdentityDialogCallback callback) {
  auto identity_update_dialog_action_for_testing =
      GetIdentityUpdateDialogActionForTesting();
  if (!identity_update_dialog_action_for_testing) {
    return;
  }

  std::move(callback).Run(identity_update_dialog_action_for_testing.value());
}

base::Value FakeWebAppUiManager::LaunchWebApp(
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    Profile& profile,
    LaunchWebAppCallback callback,
    AppLock& lock) {
  // Due to this sometimes causing confusion in tests, print that a launch has
  // been faked. To have launches create real WebContents in unit_tests (which
  // will be non-functional anyways), populate the WebAppUiManagerImpl in the
  // FakeWebAppProvider during startup.
  LOG(INFO) << "Pretending to launch web app " << params.app_id;
  std::move(callback).Run(nullptr, nullptr,
                          apps::LaunchContainer::kLaunchContainerNone);
  if (on_launch_web_app_callback_) {
    on_launch_web_app_callback_.Run(std::move(params),
                                    std::move(launch_setting));
  }
  return base::Value("FakeWebAppUiManager::LaunchWebApp");
}

#if BUILDFLAG(IS_CHROMEOS)
void FakeWebAppUiManager::MigrateLauncherState(const AppId& from_app_id,
                                               const AppId& to_app_id,
                                               base::OnceClosure callback) {
  std::move(callback).Run();
}

void FakeWebAppUiManager::DisplayRunOnOsLoginNotification(
    const std::vector<std::string>& app_names,
    base::WeakPtr<Profile> profile) {
  // Still show the notification so it can be tested using the
  // NotificationDisplayServiceTester
  web_app::DisplayRunOnOsLoginNotification(app_names, profile);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

content::WebContents* FakeWebAppUiManager::CreateNewTab() {
  return nullptr;
}

void FakeWebAppUiManager::TriggerInstallDialog(
    content::WebContents* web_contents) {}

void FakeWebAppUiManager::PresentUserUninstallDialog(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    BrowserWindow* parent_window,
    UninstallCompleteCallback callback) {
  std::move(callback).Run(webapps::UninstallResultCode::kSuccess);
}

void FakeWebAppUiManager::PresentUserUninstallDialog(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    UninstallCompleteCallback callback) {
  std::move(callback).Run(webapps::UninstallResultCode::kSuccess);
}

void FakeWebAppUiManager::PresentUserUninstallDialog(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    UninstallCompleteCallback callback,
    UninstallScheduledCallback scheduled_callback) {
  std::move(scheduled_callback).Run(/*uninstall_scheduled=*/true);
  std::move(callback).Run(webapps::UninstallResultCode::kSuccess);
}

}  // namespace web_app
