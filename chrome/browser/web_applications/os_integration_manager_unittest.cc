// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace web_app {
namespace {

class MockOsIntegrationManager : public OsIntegrationManager {
 public:
  MockOsIntegrationManager()
      : OsIntegrationManager(nullptr, nullptr, nullptr, nullptr, nullptr) {}
  explicit MockOsIntegrationManager(
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager)
      : OsIntegrationManager(nullptr,
                             nullptr,
                             nullptr,
                             std::move(protocol_handler_manager),
                             nullptr) {}
  ~MockOsIntegrationManager() override = default;

  // Installation:
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
              RegisterProtocolHandlers,
              (const AppId& app_id,
               base::OnceCallback<void(bool success)> callback),
              (override));
  MOCK_METHOD(void,
              RegisterUrlHandlers,
              (const AppId& app_id,
               base::OnceCallback<void(bool success)> callback),
              (override));
  MOCK_METHOD(void,
              RegisterShortcutsMenu,
              (const AppId& app_id,
               const std::vector<WebApplicationShortcutsMenuItemInfo>&
                   shortcuts_menu_item_infos,
               const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
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

  // Uninstallation:
  MOCK_METHOD(bool, UnregisterShortcutsMenu, (const AppId& app_id), (override));
  MOCK_METHOD(void,
              UnregisterRunOnOsLogin,
              (const AppId& app_id,
               const base::FilePath& profile_path,
               const std::u16string& shortcut_title,
               UnregisterRunOnOsLoginCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteShortcuts,
              (const AppId& app_id,
               const base::FilePath& shortcuts_data_dir,
               std::unique_ptr<ShortcutInfo> shortcut_info,
               DeleteShortcutsCallback callback),
              (override));
  MOCK_METHOD(void,
              UnregisterFileHandlers,
              (const AppId& app_id, base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              UnregisterProtocolHandlers,
              (const AppId& app_id, base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void, UnregisterUrlHandlers, (const AppId& app_id), (override));
  MOCK_METHOD(void,
              UnregisterWebAppOsUninstallation,
              (const AppId& app_id),
              (override));

  // Update:
  MOCK_METHOD(void,
              UpdateFileHandlers,
              (const AppId& app_id,
               FileHandlerUpdateAction file_handlers_need_os_update),
              (override));
  MOCK_METHOD(void,
              UpdateShortcuts,
              (const AppId& app_id,
               base::StringPiece old_name,
               base::OnceClosure callback),
              (override));
  MOCK_METHOD(void,
              UpdateShortcutsMenu,
              (const AppId& app_id, const WebApplicationInfo& web_app_info),
              (override));
  MOCK_METHOD(void,
              UpdateUrlHandlers,
              (const AppId& app_id,
               base::OnceCallback<void(bool success)> callback),
              (override));
  MOCK_METHOD(void,
              UpdateProtocolHandlers,
              (const AppId& app_id,
               bool force_shortcut_updates_if_needed,
               base::OnceClosure update_finished_callback),
              (override));

  // Utility methods:
  MOCK_METHOD(std::unique_ptr<ShortcutInfo>,
              BuildShortcutInfo,
              (const AppId& app_id),
              (override));
};

#if defined(OS_WIN)
const base::FilePath::CharType kFakeProfilePath[] =
    FILE_PATH_LITERAL("\\profile\\path");
#else
const base::FilePath::CharType kFakeProfilePath[] =
    FILE_PATH_LITERAL("/profile/path");
#endif  // defined(OS_WIN)

const char kFakeAppUrl[] = "https://fake.com";
const std::u16string kFakeAppTitle(u"fake title");

std::unique_ptr<ShortcutInfo> CreateTestShorcutInfo(const AppId& app_id) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->profile_path = base::FilePath(kFakeProfilePath);
  shortcut_info->extension_id = app_id;
  shortcut_info->url = GURL(kFakeAppUrl);
  shortcut_info->title = kFakeAppTitle;
  return shortcut_info;
}

class OsIntegrationManagerTest : public testing::Test {
 public:
  OsIntegrationManagerTest() {
    features_.InitWithFeatures({blink::features::kWebAppEnableUrlHandlers,
                                ::features::kDesktopPWAsRunOnOsLogin},
                               {});
  }

  ~OsIntegrationManagerTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

TEST_F(OsIntegrationManagerTest, InstallOsHooksOnlyShortcuts) {
  base::RunLoop run_loop;

  OsHooksErrors install_errors;
  InstallOsHooksCallback callback =
      base::BindLambdaForTesting([&](OsHooksErrors errors) {
        install_errors = errors;
        run_loop.Quit();
      });

  const AppId app_id = "test";

  testing::StrictMock<MockOsIntegrationManager> manager;
  EXPECT_CALL(manager, MacAppShimOnAppInstalledForProfile(app_id)).Times(1);
  EXPECT_CALL(manager, CreateShortcuts(app_id, false, testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(true));

  InstallOsHooksOptions options;
  options.os_hooks[OsHookType::kShortcuts] = true;
  manager.InstallOsHooks(app_id, std::move(callback), nullptr, options);
  run_loop.Run();
  EXPECT_FALSE(install_errors[OsHookType::kShortcuts]);
}

TEST_F(OsIntegrationManagerTest, InstallOsHooksEverything) {
  base::RunLoop run_loop;

  OsHooksErrors install_errors;
  InstallOsHooksCallback callback =
      base::BindLambdaForTesting([&](OsHooksErrors errors) {
        install_errors = errors;
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
  EXPECT_CALL(manager, RegisterProtocolHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, RegisterUrlHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, AddAppToQuickLaunchBar(app_id)).Times(1);
  EXPECT_CALL(manager, ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
                           app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_CALL(manager, RegisterRunOnOsLogin(app_id, testing::_)).Times(1);

  InstallOsHooksOptions options;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = true;
  // Set all hooks to true.
  options.os_hooks.set();
  manager.InstallOsHooks(app_id, std::move(callback), nullptr, options);
  run_loop.Run();
  EXPECT_FALSE(install_errors[OsHookType::kShortcuts]);
  EXPECT_FALSE(install_errors[OsHookType::kFileHandlers]);
  EXPECT_FALSE(install_errors[OsHookType::kProtocolHandlers]);
  EXPECT_FALSE(install_errors[OsHookType::kUrlHandlers]);
  EXPECT_FALSE(install_errors[OsHookType::kRunOnOsLogin]);
  // Note: We asked for these to be installed, but their methods were not
  // called. This is because the features are turned off. We only set these
  // results to true if there is an unexpected error, so they remain false.
  EXPECT_FALSE(install_errors[OsHookType::kShortcutsMenu]);
  EXPECT_FALSE(install_errors[OsHookType::kUninstallationViaOsSettings]);
}

TEST_F(OsIntegrationManagerTest, UninstallOsHooksEverything) {
  base::RunLoop run_loop;

  OsHooksErrors uninstall_errors;
  UninstallOsHooksCallback callback =
      base::BindLambdaForTesting([&](OsHooksErrors errors) {
        uninstall_errors = errors;
        run_loop.Quit();
      });

  const AppId app_id = "test";

  const base::FilePath kExpectedShortcutPath =
      base::FilePath(kFakeProfilePath)
          .Append(chrome::kWebAppDirname)
          .AppendASCII("_crx_test");

  testing::StrictMock<MockOsIntegrationManager> manager;
  EXPECT_CALL(manager, BuildShortcutInfo(app_id))
      .WillOnce(
          testing::Return(testing::ByMove(CreateTestShorcutInfo(app_id))));
  EXPECT_CALL(manager, DeleteShortcuts(app_id, kExpectedShortcutPath,
                                       testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(true));
  EXPECT_CALL(manager, UnregisterFileHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, UnregisterProtocolHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, UnregisterUrlHandlers(app_id)).Times(1);
  EXPECT_CALL(manager, UnregisterWebAppOsUninstallation(app_id)).Times(1);
  EXPECT_CALL(manager, UnregisterShortcutsMenu(app_id))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(manager,
              UnregisterRunOnOsLogin(app_id, base::FilePath(kFakeProfilePath),
                                     kFakeAppTitle, testing::_))
      .Times(1);

  manager.UninstallAllOsHooks(app_id, std::move(callback));
  run_loop.Run();
  EXPECT_FALSE(uninstall_errors[OsHookType::kShortcuts]);
  EXPECT_FALSE(uninstall_errors[OsHookType::kFileHandlers]);
  EXPECT_FALSE(uninstall_errors[OsHookType::kProtocolHandlers]);
  EXPECT_FALSE(uninstall_errors[OsHookType::kUrlHandlers]);
  EXPECT_FALSE(uninstall_errors[OsHookType::kRunOnOsLogin]);
  EXPECT_FALSE(uninstall_errors[OsHookType::kShortcutsMenu]);
  EXPECT_FALSE(uninstall_errors[OsHookType::kUninstallationViaOsSettings]);
}

TEST_F(OsIntegrationManagerTest, UpdateOsHooksEverything) {
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;

  WebApplicationInfo web_app_info;
  base::StringPiece old_name = "test-name";

  EXPECT_CALL(manager,
              UpdateFileHandlers(app_id, FileHandlerUpdateAction::kUpdate))
      .Times(1);
  EXPECT_CALL(manager, UpdateShortcuts(app_id, old_name, testing::_)).Times(1);
  EXPECT_CALL(manager, UpdateShortcutsMenu(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, UpdateUrlHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, UpdateProtocolHandlers(app_id, false, testing::_))
      .Times(1);

  manager.UpdateOsHooks(app_id, old_name, FileHandlerUpdateAction::kUpdate,
                        web_app_info);
}

TEST_F(OsIntegrationManagerTest, UpdateProtocolHandlers) {
#if defined(OS_WIN)
  // UpdateProtocolHandlers is a no-op on Win7
  if (base::win::GetVersion() == base::win::Version::WIN7)
    return;
#endif

  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager(
      std::make_unique<WebAppProtocolHandlerManager>(nullptr));
  base::RunLoop run_loop;

#if !defined(OS_WIN)
  EXPECT_CALL(manager, UpdateShortcuts(app_id, base::StringPiece(), testing::_))
      .WillOnce([](const AppId& app_id, base::StringPiece old_name,
                   base::OnceClosure update_finished_callback) {
        std::move(update_finished_callback).Run();
      });
#endif

  EXPECT_CALL(manager, UnregisterProtocolHandlers(app_id, testing::_))
      .WillOnce([](const AppId& app_id,
                   base::OnceCallback<void(bool)> update_finished_callback) {
        std::move(update_finished_callback).Run(true);
      });

  EXPECT_CALL(manager, RegisterProtocolHandlers(app_id, testing::_))
      .WillOnce([](const AppId& app_id,
                   base::OnceCallback<void(bool)> update_finished_callback) {
        std::move(update_finished_callback).Run(true);
      });

  auto update_finished_callback =
      base::BindLambdaForTesting([&]() { run_loop.Quit(); });

  manager.OsIntegrationManager::UpdateProtocolHandlers(
      app_id, true, update_finished_callback);
  run_loop.Run();
}

}  // namespace
}  // namespace web_app
