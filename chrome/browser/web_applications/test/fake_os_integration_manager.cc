// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

FakeOsIntegrationManager::FakeOsIntegrationManager(
    Profile* profile,
    std::unique_ptr<WebAppShortcutManager> shortcut_manager,
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager)
    : OsIntegrationManager(profile,
                           std::move(shortcut_manager),
                           std::move(file_handler_manager),
                           std::move(protocol_handler_manager)),
      scoped_suppress_(
          std::make_unique<OsIntegrationManager::ScopedSuppressForTesting>()) {
  if (!this->shortcut_manager()) {
    set_shortcut_manager(std::make_unique<TestShortcutManager>(profile));
  }
  if (!has_file_handler_manager()) {
    set_file_handler_manager(
        std::make_unique<FakeWebAppFileHandlerManager>(profile));
  }
}

FakeOsIntegrationManager::~FakeOsIntegrationManager() = default;

void FakeOsIntegrationManager::SetFileHandlerManager(
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager) {
  set_file_handler_manager(std::move(file_handler_manager));
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
