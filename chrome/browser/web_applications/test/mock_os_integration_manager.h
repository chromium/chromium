// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_OS_INTEGRATION_MANAGER_H_

#include <memory>

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app {

class WebAppProtocolHandlerManager;

class MockOsIntegrationManager : public OsIntegrationManager {
 public:
  MockOsIntegrationManager();
  explicit MockOsIntegrationManager(
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager);
  ~MockOsIntegrationManager() override;

  void SetProvider(base::PassKey<WebAppProvider>,
                   WebAppProvider& provider) override;
  void Start() override {}

  // Installation:
  MOCK_METHOD(void,
              Synchronize,
              (const AppId& app_id,
               base::OnceClosure callback,
               absl::optional<SynchronizeOsOptions> options),
              (override));

  MOCK_METHOD(void,
              CreateShortcuts,
              (const AppId& app_id,
               bool add_to_desktop,
               ShortcutCreationReason reason,
               CreateShortcutsCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterFileHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterProtocolHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              RegisterUrlHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              RegisterShortcutsMenu,
              (const AppId& app_id,
               const std::vector<WebAppShortcutsMenuItemInfo>&
                   shortcuts_menu_item_infos,
               const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu,
              (const AppId& app_id, ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterRunOnOsLogin,
              (const AppId& app_id, ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              MacAppShimOnAppInstalledForProfile,
              (const AppId& app_id),
              (override));

  MOCK_METHOD(void, AddAppToQuickLaunchBar, (const AppId& app_id), (override));

  MOCK_METHOD(void,
              RegisterWebAppOsUninstallation,
              (const AppId& app_id, const std::string& name),
              (override));

  // Uninstallation:
  MOCK_METHOD(void,
              UninstallAllOsHooks,
              (const AppId& app_id, UninstallOsHooksCallback callback),
              (override));
  MOCK_METHOD(bool,
              UnregisterShortcutsMenu,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              UnregisterRunOnOsLogin,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteShortcuts,
              (const AppId& app_id,
               const base::FilePath& shortcuts_data_dir,
               std::unique_ptr<ShortcutInfo> shortcut_info,
               ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              UnregisterFileHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              UnregisterProtocolHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void, UnregisterUrlHandlers, (const AppId& app_id), (override));
  MOCK_METHOD(void,
              UnregisterWebAppOsUninstallation,
              (const AppId& app_id),
              (override));

  // Update:
  MOCK_METHOD(void,
              UpdateShortcuts,
              (const AppId& app_id,
               base::StringPiece old_name,
               ResultCallback callback),
              (override));

  // Utility methods:
  MOCK_METHOD(std::unique_ptr<ShortcutInfo>,
              BuildShortcutInfo,
              (const AppId& app_id),
              (override));
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_OS_INTEGRATION_MANAGER_H_
