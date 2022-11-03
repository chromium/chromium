// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
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

bool FakeWebAppUiManager::DidUninstallAndReplace(const AppId& from_app,
                                                 const AppId& to_app) {
  return uninstall_and_replace_map_[from_app] == to_app;
}

void FakeWebAppUiManager::ResolveAppIdentityDialogForTesting(bool enabled) {
  resolve_app_identity_dialog_for_testing_ = enabled;
}

WebAppUiManagerImpl* FakeWebAppUiManager::AsImpl() {
  return nullptr;
}

size_t FakeWebAppUiManager::GetNumWindowsForApp(const AppId& app_id) {
  DCHECK(base::Contains(app_id_to_num_windows_map_, app_id));
  return app_id_to_num_windows_map_[app_id];
}

void FakeWebAppUiManager::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&, app_id, callback = std::move(callback)]() mutable {
                       app_id_to_num_windows_map_[app_id] = 0;
                       std::move(callback).Run();
                     }));
}

bool FakeWebAppUiManager::UninstallAndReplaceIfExists(
    const std::vector<AppId>& from_apps,
    const AppId& to_app) {
  for (const AppId& from_app : from_apps) {
    uninstall_and_replace_map_[from_app] = to_app;
  }
  return false;
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
  if (!resolve_app_identity_dialog_for_testing_.has_value())
    return;

  if (resolve_app_identity_dialog_for_testing_.value())
    std::move(callback).Run(web_app::AppIdentityUpdate::kAllowed);
  else
    std::move(callback).Run(web_app::AppIdentityUpdate::kSkipped);
}

}  // namespace web_app
