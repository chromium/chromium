// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_os_integration_manager.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"

namespace web_app {
TestOsIntegrationManager::TestOsIntegrationManager(
    Profile* profile,
    std::unique_ptr<AppShortcutManager> shortcut_manager,
    std::unique_ptr<FileHandlerManager> file_handler_manager)
    : OsIntegrationManager(profile,
                           std::move(shortcut_manager),
                           std::move(file_handler_manager)) {
  if (!this->shortcut_manager()) {
    set_shortcut_manager(std::make_unique<TestShortcutManager>(profile));
  }
  if (!this->file_handler_manager()) {
    set_file_handler_manager(std::make_unique<TestFileHandlerManager>(profile));
  }
}

TestOsIntegrationManager::~TestOsIntegrationManager() = default;

void TestOsIntegrationManager::SetNextCreateShortcutsResult(const AppId& app_id,
                                                            bool success) {
  DCHECK(!base::Contains(next_create_shortcut_results_, app_id));
  next_create_shortcut_results_[app_id] = success;
}

void TestOsIntegrationManager::InstallOsHooks(
    const AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options) {
  OsHooksResults os_hooks_results{false};
  os_hooks_results[OsHookType::kFileHandlers] = true;
  os_hooks_results[OsHookType::kShortcutsMenu] = true;

  did_add_to_desktop_ = options.add_to_desktop;

  if (options.os_hooks[OsHookType::kShortcuts] && can_create_shortcuts_) {
    bool success = true;
    auto it = next_create_shortcut_results_.find(app_id);
    if (it != next_create_shortcut_results_.end()) {
      success = it->second;
      next_create_shortcut_results_.erase(app_id);
    }
    if (success) {
      ++num_create_shortcuts_calls_;
      os_hooks_results[OsHookType::kShortcutsMenu] = true;
    }
  }

  if (options.os_hooks[OsHookType::kRunOnOsLogin]) {
    ++num_register_run_on_os_login_calls_;
    os_hooks_results[OsHookType::kRunOnOsLogin] = true;
  }

  if (options.add_to_quick_launch_bar)
    ++num_add_app_to_quick_launch_bar_calls_;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(os_hooks_results)));
}

void TestOsIntegrationManager::UninstallOsHooks(
    const AppId& app_id,
    const OsHooksResults& os_hooks,
    UninstallOsHooksCallback callback) {
  NOTIMPLEMENTED();
}

void TestOsIntegrationManager::UninstallAllOsHooks(
    const AppId& app_id,
    UninstallOsHooksCallback callback) {
  NOTIMPLEMENTED();
}

void TestOsIntegrationManager::UpdateOsHooks(
    const AppId& app_id,
    base::StringPiece old_name,
    const WebApplicationInfo& web_app_info) {
  NOTIMPLEMENTED();
}

void TestOsIntegrationManager::SetFileHandlerManager(
    std::unique_ptr<FileHandlerManager> file_handler_manager) {
  set_file_handler_manager(std::move(file_handler_manager));
}

TestShortcutManager::TestShortcutManager(Profile* profile)
    : AppShortcutManager(profile) {}

TestShortcutManager::~TestShortcutManager() = default;

std::unique_ptr<ShortcutInfo> TestShortcutManager::BuildShortcutInfo(
    const AppId& app_id) {
  return nullptr;
}

void TestShortcutManager::GetShortcutInfoForApp(
    const AppId& app_id,
    GetShortcutInfoCallback callback) {
  std::move(callback).Run(nullptr);
}
}  // namespace web_app
