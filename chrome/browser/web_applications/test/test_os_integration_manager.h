// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_OS_INTEGRATION_MANAGER_H_

#include <map>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

class AppShortcutManager;
class FileHandlerManager;
class ProtocolHandlerManager;
class UrlHandlerManager;

class TestOsIntegrationManager : public OsIntegrationManager {
 public:
  TestOsIntegrationManager(
      Profile* profile,
      std::unique_ptr<AppShortcutManager> shortcut_manager,
      std::unique_ptr<FileHandlerManager> file_handler_manager,
      std::unique_ptr<ProtocolHandlerManager> protocol_handler_manager,
      std::unique_ptr<UrlHandlerManager> url_handler_manager);
  ~TestOsIntegrationManager() override;

  // OsIntegrationManager:
  void InstallOsHooks(const AppId& app_id,
                      InstallOsHooksCallback callback,
                      std::unique_ptr<WebApplicationInfo> web_app_info,
                      InstallOsHooksOptions options) override;
  void UninstallOsHooks(const AppId& app_id,
                        const OsHooksResults& os_hooks,
                        UninstallOsHooksCallback callback) override;
  void UninstallAllOsHooks(const AppId& app_id,
                           UninstallOsHooksCallback callback) override;
  void UpdateOsHooks(const AppId& app_id,
                     base::StringPiece old_name,
                     std::unique_ptr<ShortcutInfo> old_shortcut,
                     bool file_handlers_need_os_update,
                     const WebApplicationInfo& web_app_info) override;

  size_t num_create_shortcuts_calls() const {
    return num_create_shortcuts_calls_;
  }

  size_t num_create_file_handlers_calls() const {
    return num_create_file_handlers_calls_;
  }

  size_t num_register_run_on_os_login_calls() const {
    return num_register_run_on_os_login_calls_;
  }

  size_t num_add_app_to_quick_launch_bar_calls() const {
    return num_add_app_to_quick_launch_bar_calls_;
  }

  size_t num_register_url_handlers_calls() const {
    return num_register_url_handlers_calls_;
  }

  void set_can_create_shortcuts(bool can_create_shortcuts) {
    can_create_shortcuts_ = can_create_shortcuts;
  }

  base::Optional<bool> did_add_to_desktop() const {
    return did_add_to_desktop_;
  }

  base::Optional<InstallOsHooksOptions> get_last_install_options() const {
    return last_options_;
  }

  void SetNextCreateShortcutsResult(const AppId& app_id, bool success);

  void SetFileHandlerManager(
      std::unique_ptr<FileHandlerManager> file_handler_manager);

  void SetProtocolHandlerManager(
      std::unique_ptr<ProtocolHandlerManager> protocol_handler_manager);

  void SetUrlHandlerManager(
      std::unique_ptr<UrlHandlerManager> url_handler_manager);

  TestOsIntegrationManager* AsTestOsIntegrationManager() override;

 private:
  size_t num_create_shortcuts_calls_ = 0;
  size_t num_create_file_handlers_calls_ = 0;
  size_t num_register_run_on_os_login_calls_ = 0;
  size_t num_add_app_to_quick_launch_bar_calls_ = 0;
  size_t num_register_url_handlers_calls_ = 0;
  base::Optional<bool> did_add_to_desktop_;
  base::Optional<InstallOsHooksOptions> last_options_;

  bool can_create_shortcuts_ = true;
  std::map<AppId, bool> next_create_shortcut_results_;
};

// Stub test shortcut manager.
class TestShortcutManager : public AppShortcutManager {
 public:
  explicit TestShortcutManager(Profile* profile);
  ~TestShortcutManager() override;
  std::unique_ptr<ShortcutInfo> BuildShortcutInfo(const AppId& app_id) override;
  void SetShortcutInfoForApp(const AppId& app_id,
                             std::unique_ptr<ShortcutInfo> shortcut_info);
  void GetShortcutInfoForApp(const AppId& app_id,
                             GetShortcutInfoCallback callback) override;
  void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info) override;
  void SetAppExistingShortcuts(GURL app_url, ShortcutLocations locations) {
    existing_shortcut_locations_[app_url] = locations;
  }

  std::map<AppId, std::unique_ptr<ShortcutInfo>> shortcut_info_map_;
  std::map<GURL, ShortcutLocations> existing_shortcut_locations_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_OS_INTEGRATION_MANAGER_H_
