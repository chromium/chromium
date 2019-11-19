// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"

#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace web_app {

TestWebAppUiManager::TestWebAppUiManager() = default;

TestWebAppUiManager::~TestWebAppUiManager() = default;

void TestWebAppUiManager::SetNumWindowsForApp(const AppId& app_id,
                                              size_t num_windows_for_app) {
  app_id_to_num_windows_map_[app_id] = num_windows_for_app;
}

bool TestWebAppUiManager::DidUninstallAndReplace(const AppId& from_app,
                                                 const AppId& to_app) {
  return uninstall_and_replace_map_[from_app] == to_app;
}

WebAppUiManagerImpl* TestWebAppUiManager::AsImpl() {
  return nullptr;
}

size_t TestWebAppUiManager::GetNumWindowsForApp(const AppId& app_id) {
  DCHECK(base::Contains(app_id_to_num_windows_map_, app_id));
  return app_id_to_num_windows_map_[app_id];
}

void TestWebAppUiManager::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&, app_id, callback = std::move(callback)]() mutable {
                       app_id_to_num_windows_map_[app_id] = 0;
                       std::move(callback).Run();
                     }));
}

void TestWebAppUiManager::UninstallAndReplace(
    const std::vector<AppId>& from_apps,
    const AppId& to_app) {
  for (const AppId& from_app : from_apps) {
    uninstall_and_replace_map_[from_app] = to_app;
  }
}

bool TestWebAppUiManager::CanAddAppToQuickLaunchBar() const {
  return false;
}

void TestWebAppUiManager::AddAppToQuickLaunchBar(const AppId& app_id) {}

bool TestWebAppUiManager::IsInAppWindow(
    content::WebContents* web_contents) const {
  return false;
}

bool TestWebAppUiManager::CanReparentAppTabToWindow(
    const AppId& app_id,
    bool shortcut_created) const {
  return false;
}

void TestWebAppUiManager::ReparentAppTabToWindow(content::WebContents* contents,
                                                 const AppId& app_id,
                                                 bool shortcut_created) {}

}  // namespace web_app
