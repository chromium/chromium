// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace web_app {
namespace {

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kFakeProfilePath[] =
    FILE_PATH_LITERAL("\\profile\\path");
#else
const base::FilePath::CharType kFakeProfilePath[] =
    FILE_PATH_LITERAL("/profile/path");
#endif  // BUILDFLAG(IS_WIN)

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
  EXPECT_CALL(manager, CreateShortcuts(app_id, false,
                                       SHORTCUT_CREATION_AUTOMATED, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(true));

  InstallOsHooksOptions options;
  options.os_hooks[OsHookType::kShortcuts] = true;
  options.reason = SHORTCUT_CREATION_AUTOMATED;
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
  EXPECT_CALL(manager, CreateShortcuts(app_id, true, SHORTCUT_CREATION_BY_USER,
                                       testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(true));
  EXPECT_CALL(manager, RegisterFileHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, RegisterProtocolHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, RegisterUrlHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, AddAppToQuickLaunchBar(app_id)).Times(1);
  EXPECT_CALL(manager, ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
                           app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(Result::kOk));
  EXPECT_CALL(manager, RegisterRunOnOsLogin(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(1);

  InstallOsHooksOptions options;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = true;
  options.reason = SHORTCUT_CREATION_BY_USER;
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
      .WillOnce(base::test::RunOnceCallback<3>(Result::kOk));
  EXPECT_CALL(manager, UnregisterFileHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, UnregisterProtocolHandlers(app_id, testing::_)).Times(1);
  EXPECT_CALL(manager, UnregisterUrlHandlers(app_id)).Times(1);
  EXPECT_CALL(manager, UnregisterWebAppOsUninstallation(app_id)).Times(1);
  EXPECT_CALL(manager, UnregisterShortcutsMenu(app_id, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(manager, UnregisterRunOnOsLogin(app_id, testing::_)).Times(1);

  EXPECT_CALL(manager, UninstallAllOsHooks(testing::_, testing::_))
      .WillOnce(
          [&manager](const AppId& app_id, UninstallOsHooksCallback callback) {
            return manager.OsIntegrationManager::UninstallAllOsHooks(
                app_id, std::move(callback));
          });

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

TEST_F(OsIntegrationManagerTest, UpdateProtocolHandlers) {
#if BUILDFLAG(IS_WIN)
  // UpdateProtocolHandlers is a no-op on Win7
  if (base::win::GetVersion() == base::win::Version::WIN7)
    return;
#endif

  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager(
      std::make_unique<WebAppProtocolHandlerManager>(nullptr));
  base::RunLoop run_loop;

#if !BUILDFLAG(IS_WIN)
  EXPECT_CALL(manager, UpdateShortcuts(app_id, base::StringPiece(), testing::_))
      .WillOnce([](const AppId& app_id, base::StringPiece old_name,
                   ResultCallback update_finished_callback) {
        std::move(update_finished_callback).Run(Result::kOk);
      });
#endif

  EXPECT_CALL(manager, UnregisterProtocolHandlers(app_id, testing::_))
      .WillOnce(
          [](const AppId& app_id, ResultCallback update_finished_callback) {
            std::move(update_finished_callback).Run(Result::kOk);
          });

  EXPECT_CALL(manager, RegisterProtocolHandlers(app_id, testing::_))
      .WillOnce(
          [](const AppId& app_id, ResultCallback update_finished_callback) {
            std::move(update_finished_callback).Run(Result::kOk);
          });

  auto update_finished_callback =
      base::BindLambdaForTesting([&]() { run_loop.Quit(); });

  manager.OsIntegrationManager::UpdateProtocolHandlers(
      app_id, true, update_finished_callback);
  run_loop.Run();
}

}  // namespace
}  // namespace web_app
