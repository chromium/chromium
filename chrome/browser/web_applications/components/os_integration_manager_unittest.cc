// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/os_integration_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {
class MockOsIntegrationManager : public OsIntegrationManager {
 public:
  MockOsIntegrationManager()
      : OsIntegrationManager(nullptr, nullptr, nullptr, nullptr) {}
  ~MockOsIntegrationManager() override = default;

  MOCK_METHOD(void,
              CreateShortcuts,
              (const AppId& app_id,
               bool add_to_desktop,
               CreateShortcutsCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterFileHandlers,
              (const AppId& app_id,
               base::OnceCallback<void(bool success)> callback),
              (override));

  MOCK_METHOD(void,
              RegisterShortcutsMenu,
              (const AppId& app_id,
               const std::vector<WebApplicationShortcutsMenuItemInfo>&
                   shortcuts_menu_item_infos,
               const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps,
               base::OnceCallback<void(bool success)> callback),
              (override));

  MOCK_METHOD(void,
              ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu,
              (const AppId& app_id,
               base::OnceCallback<void(bool success)> callback),
              (override));

  MOCK_METHOD(void,
              RegisterRunOnOsLogin,
              (const AppId& app_id, RegisterRunOnOsLoginCallback callback),
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
};

TEST(OsIntegrationManagerTest, InstallOsHooksOnlyShortcuts) {
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  base::RunLoop run_loop;

  OsHooksResults install_results;
  InstallOsHooksCallback callback =
      base::BindLambdaForTesting([&](OsHooksResults results) {
        install_results = results;
        run_loop.Quit();
      });

  const AppId app_id = "test";

  testing::StrictMock<MockOsIntegrationManager> manager;
  EXPECT_CALL(manager, MacAppShimOnAppInstalledForProfile(app_id)).Times(1);
  EXPECT_CALL(manager, CreateShortcuts(app_id, false, testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(true));

  InstallOsHooksOptions options;
  options.os_hooks = OsHooksResults{false};
  options.os_hooks[OsHookType::kShortcuts] = true;
  manager.InstallOsHooks(app_id, std::move(callback), nullptr, options);
  run_loop.Run();
  EXPECT_TRUE(install_results[OsHookType::kShortcuts]);
}

TEST(OsIntegrationManagerTest, InstallOsHooksEverything) {
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  base::RunLoop run_loop;

  OsHooksResults install_results;
  InstallOsHooksCallback callback =
      base::BindLambdaForTesting([&](OsHooksResults results) {
        install_results = results;
        run_loop.Quit();
      });

  const AppId app_id = "test";

  // Note - when features are enabled by default, more calls will needed to be
  // added here.
  testing::StrictMock<MockOsIntegrationManager> manager;
  EXPECT_CALL(manager, MacAppShimOnAppInstalledForProfile(app_id)).Times(1);
  EXPECT_CALL(manager, CreateShortcuts(app_id, true, testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(true));
  EXPECT_CALL(manager, RegisterFileHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, AddAppToQuickLaunchBar(app_id)).Times(1);
  EXPECT_CALL(manager, ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
                           app_id, testing::_))
      .Times(1);

  InstallOsHooksOptions options;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = true;
  // Set all hooks to true.
  options.os_hooks.set();
  manager.InstallOsHooks(app_id, std::move(callback), nullptr, options);
  run_loop.Run();
  EXPECT_TRUE(install_results[OsHookType::kShortcuts]);
  EXPECT_TRUE(install_results[OsHookType::kFileHandlers]);
  EXPECT_TRUE(install_results[OsHookType::kRunOnOsLogin]);
  // Note: We asked for these to be installed, but their methods were not
  // called. This is because the features are turned off. We only set these
  // results to false if there is an unexpected error, so they remain true.
  EXPECT_TRUE(install_results[OsHookType::kShortcutsMenu]);
  EXPECT_TRUE(install_results[OsHookType::kUninstallationViaOsSettings]);
}
}  // namespace web_app
