// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/url_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/test/fake_url_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

FakeOsIntegrationManager::FakeOsIntegrationManager(
    Profile* profile,
    std::unique_ptr<WebAppShortcutManager> shortcut_manager,
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
    std::unique_ptr<UrlHandlerManager> url_handler_manager)
    : OsIntegrationManager(profile,
                           std::move(shortcut_manager),
                           std::move(file_handler_manager),
                           std::move(protocol_handler_manager),
                           std::move(url_handler_manager)) {
  if (!this->shortcut_manager()) {
    set_shortcut_manager(std::make_unique<TestShortcutManager>(profile));
  }
  if (!has_file_handler_manager()) {
    set_file_handler_manager(
        std::make_unique<FakeWebAppFileHandlerManager>(profile));
  }
  if (!this->url_handler_manager()) {
    set_url_handler_manager(std::make_unique<FakeUrlHandlerManager>(profile));
  }
}

FakeOsIntegrationManager::~FakeOsIntegrationManager() = default;

void FakeOsIntegrationManager::SetNextCreateShortcutsResult(
    const webapps::AppId& app_id,
    bool success) {
  CHECK(!base::Contains(next_create_shortcut_results_, app_id));
  next_create_shortcut_results_[app_id] = success;
}

void FakeOsIntegrationManager::InstallOsHooks(
    const webapps::AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    InstallOsHooksOptions options) {
  OsHooksErrors os_hooks_errors;

  last_options_ = options;

  if (options.os_hooks[OsHookType::kFileHandlers]) {
    ++num_create_file_handlers_calls_;
  }

  did_add_to_desktop_ = options.add_to_desktop;

  if (options.os_hooks[OsHookType::kShortcuts] && can_create_shortcuts_) {
    bool success = true;
    ++num_create_shortcuts_calls_;
    auto it = next_create_shortcut_results_.find(app_id);
    if (it != next_create_shortcut_results_.end()) {
      success = it->second;
      next_create_shortcut_results_.erase(app_id);
    }
    if (!success)
      os_hooks_errors[OsHookType::kShortcutsMenu] = true;
  }

  if (options.os_hooks[OsHookType::kRunOnOsLogin]) {
    ++num_register_run_on_os_login_calls_;
  }

  if (options.add_to_quick_launch_bar)
    ++num_add_app_to_quick_launch_bar_calls_;

  if (options.os_hooks[OsHookType::kUrlHandlers]) {
    ++num_register_url_handlers_calls_;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
}

void FakeOsIntegrationManager::UninstallOsHooks(
    const webapps::AppId& app_id,
    const OsHooksOptions& os_hooks,
    UninstallOsHooksCallback callback) {
  if (os_hooks[OsHookType::kRunOnOsLogin]) {
    ++num_unregister_run_on_os_login_calls_;
  }
  OsHooksErrors os_hooks_errors;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
}

void FakeOsIntegrationManager::UninstallAllOsHooks(
    const webapps::AppId& app_id,
    UninstallOsHooksCallback callback) {
  OsHooksOptions os_hooks;
  os_hooks.set();
  UninstallOsHooks(app_id, os_hooks, std::move(callback));
}

void FakeOsIntegrationManager::UpdateOsHooks(
    const webapps::AppId& app_id,
    base::StringPiece old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebAppInstallInfo& web_app_info,
    UninstallOsHooksCallback callback) {
  if (file_handlers_need_os_update != FileHandlerUpdateAction::kNoUpdate)
    ++num_update_file_handlers_calls_;

  OsHooksErrors os_hooks_errors;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
}

void FakeOsIntegrationManager::Synchronize(
    const webapps::AppId& app_id,
    base::OnceClosure callback,
    absl::optional<SynchronizeOsOptions> options) {
  // Holding a scoped_supress ensures that execution is skipped during the
  // entire Synchronization flow. See
  // OsIntegrationManager::StartSubManagerExecutionIfRequired() for more
  // information.
  auto scoped_supress =
      std::make_unique<OsIntegrationManager::ScopedSuppressForTesting>();
  auto scoped_supress_callback = base::BindOnce(
      [&](std::unique_ptr<OsIntegrationManager::ScopedSuppressForTesting>
              scoped_supress) {},
      std::move(scoped_supress));
  OsIntegrationManager::Synchronize(
      app_id, std::move(callback).Then(std::move(scoped_supress_callback)),
      options);
}

void FakeOsIntegrationManager::SetFileHandlerManager(
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager) {
  set_file_handler_manager(std::move(file_handler_manager));
}

void FakeOsIntegrationManager::SetUrlHandlerManager(
    std::unique_ptr<UrlHandlerManager> url_handler_manager) {
  set_url_handler_manager(std::move(url_handler_manager));
}

void FakeOsIntegrationManager::SetShortcutManager(
    std::unique_ptr<WebAppShortcutManager> shortcut_manager) {
  set_shortcut_manager(std::move(shortcut_manager));
}

FakeOsIntegrationManager*
FakeOsIntegrationManager::AsTestOsIntegrationManager() {
  return this;
}

TestShortcutManager::TestShortcutManager(Profile* profile)
    : WebAppShortcutManager(profile, nullptr, nullptr) {}

TestShortcutManager::~TestShortcutManager() = default;

std::unique_ptr<ShortcutInfo> TestShortcutManager::BuildShortcutInfo(
    const webapps::AppId& app_id) {
  return nullptr;
}

void TestShortcutManager::SetShortcutInfoForApp(
    const webapps::AppId& app_id,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  shortcut_info_map_[app_id] = std::move(shortcut_info);
}

void TestShortcutManager::GetShortcutInfoForApp(
    const webapps::AppId& app_id,
    GetShortcutInfoCallback callback) {
  if (shortcut_info_map_.find(app_id) != shortcut_info_map_.end()) {
    std::move(callback).Run(std::move(shortcut_info_map_[app_id]));
    shortcut_info_map_.erase(app_id);
  } else {
    std::move(callback).Run(nullptr);
  }
}

void TestShortcutManager::GetAppExistingShortCutLocation(
    ShortcutLocationCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  ShortcutLocations locations;
  if (existing_shortcut_locations_.find(shortcut_info->url) !=
      existing_shortcut_locations_.end()) {
    locations = existing_shortcut_locations_[shortcut_info->url];
  }
  std::move(callback).Run(locations);
}

}  // namespace web_app
