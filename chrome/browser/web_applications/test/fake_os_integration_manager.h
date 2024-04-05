// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_OS_INTEGRATION_MANAGER_H_

#include <map>
#include <optional>
#include <string_view>

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebAppShortcutManager;
class WebAppFileHandlerManager;
class WebAppProtocolHandlerManager;

class FakeOsIntegrationManager : public OsIntegrationManager {
 public:
  FakeOsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppShortcutManager> shortcut_manager,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager);
  ~FakeOsIntegrationManager() override;

  void SetNextCreateShortcutsResult(const webapps::AppId& app_id, bool success);

  void SetFileHandlerManager(
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager);

  void SetShortcutManager(
      std::unique_ptr<WebAppShortcutManager> shortcut_manager);

  FakeOsIntegrationManager* AsTestOsIntegrationManager() override;

 private:
  std::unique_ptr<OsIntegrationManager::ScopedSuppressForTesting>
      scoped_suppress_;
};

// Stub test shortcut manager.
class TestShortcutManager : public WebAppShortcutManager {
 public:
  explicit TestShortcutManager(Profile* profile);
  ~TestShortcutManager() override;
  std::unique_ptr<ShortcutInfo> BuildShortcutInfo(
      const webapps::AppId& app_id) override;
  void SetShortcutInfoForApp(const webapps::AppId& app_id,
                             std::unique_ptr<ShortcutInfo> shortcut_info);
  void GetShortcutInfoForApp(const webapps::AppId& app_id,
                             GetShortcutInfoCallback callback) override;
  void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info) override;
  void SetAppExistingShortcuts(const GURL& app_url,
                               ShortcutLocations locations) {
    existing_shortcut_locations_[app_url] = locations;
  }

  std::map<webapps::AppId, std::unique_ptr<ShortcutInfo>> shortcut_info_map_;
  std::map<GURL, ShortcutLocations> existing_shortcut_locations_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_OS_INTEGRATION_MANAGER_H_
