// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

FakeWebAppUiManager::FakeWebAppUiManager() = default;

FakeWebAppUiManager::~FakeWebAppUiManager() = default;

void FakeWebAppUiManager::SetSubsystems(
    WebAppSyncBridge* sync_bridge,
    OsIntegrationManager* os_integration_manager) {}

void FakeWebAppUiManager::Start() {}

void FakeWebAppUiManager::Shutdown() {}

void FakeWebAppUiManager::SetNumWindowsForApp(const AppId& app_id,
                                              size_t num_windows_for_app) {
  app_id_to_num_windows_map_[app_id] = num_windows_for_app;
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&, app_id, callback = std::move(callback)]() mutable {
                       app_id_to_num_windows_map_[app_id] = 0;
                       std::move(callback).Run();
                     }));
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

void FakeWebAppUiManager::MaybeTransferAppAttributes(
    const AppId& from_extension_or_app,
    const AppId& to_app) {}

content::WebContents* FakeWebAppUiManager::CreateNewTab() {
  return nullptr;
}

void FakeWebAppUiManager::TriggerInstallDialog(
    content::WebContents* web_contents) {}

}  // namespace web_app
