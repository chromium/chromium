// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/mock_app_home_page.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(OS_WIN)

using web_app::AppId;
using GetAppsCallback =
    base::OnceCallback<void(std::vector<app_home::mojom::AppInfoPtr>)>;

namespace webapps {

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestAppName[] = "Test App";

#if !BUILDFLAG(IS_MAC)
void FlushShortcutTasks() {
  // Execute the UI thread task runner before and after the shortcut task runner
  // to ensure that tasks get to the shortcut runner, and then any scheduled
  // replies on the UI thread get run.
  {
    base::RunLoop loop;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  {
    base::RunLoop loop;
    web_app::internals::GetShortcutIOTaskRunner()->PostTask(FROM_HERE,
                                                            loop.QuitClosure());
    loop.Run();
  }
  {
    base::RunLoop loop;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
}
#endif

class TestAppHomePageHandler : public AppHomePageHandler {
 public:
  TestAppHomePageHandler(content::WebUI* web_ui,
                         Profile* profile,
                         mojo::PendingRemote<app_home::mojom::Page> page)
      : AppHomePageHandler(
            web_ui,
            profile,
            mojo::PendingReceiver<app_home::mojom::PageHandler>(),
            std::move(page)) {
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  TestAppHomePageHandler(const TestAppHomePageHandler&) = delete;
  TestAppHomePageHandler& operator=(const TestAppHomePageHandler&) = delete;

  ~TestAppHomePageHandler() override = default;

  void Wait() {
    // TODO(crbug.com/1350406): Define specific Wait for each
    // listener.
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForRunOnOsLoginModeChanged(base::OnceClosure handle) {
    run_on_os_login_mode_changed_handle_ = std::move(handle);
  }

 private:
  void OnWebAppInstalled(const web_app::AppId& app_id) override {
    run_loop_->Quit();
    AppHomePageHandler::OnWebAppInstalled(app_id);
  }

  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override {
    run_loop_->Quit();
    AppHomePageHandler::OnWebAppWillBeUninstalled(app_id);
  }

  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    run_loop_->Quit();
    AppHomePageHandler::OnExtensionLoaded(browser_context, extension);
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    run_loop_->Quit();
    AppHomePageHandler::OnExtensionUninstalled(browser_context, extension,
                                               reason);
  }

  void OnWebAppRunOnOsLoginModeChanged(
      const web_app::AppId& app_id,
      web_app::RunOnOsLoginMode run_on_os_login_mode) override {
    std::move(run_on_os_login_mode_changed_handle_).Run();
    AppHomePageHandler::OnWebAppRunOnOsLoginModeChanged(app_id,
                                                        run_on_os_login_mode);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure run_on_os_login_mode_changed_handle_;
};

std::unique_ptr<WebAppInstallInfo> BuildWebAppInfo() {
  auto app_info = std::make_unique<WebAppInstallInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = base::UTF8ToUTF16(base::StringPiece(kTestAppName));
  app_info->manifest_url = GURL(kTestManifestUrl);

  return app_info;
}

GetAppsCallback WrapGetAppsCallback(
    std::vector<app_home::mojom::AppInfoPtr>* out,
    base::OnceClosure quit_closure) {
  return base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<app_home::mojom::AppInfoPtr>* out,
         std::vector<app_home::mojom::AppInfoPtr> result) {
        *out = std::move(result);
        std::move(quit_closure).Run();
      },
      std::move(quit_closure), out);
}

}  // namespace

class AppHomePageHandlerTest : public InProcessBrowserTest {
 public:
  AppHomePageHandlerTest() = default;

  AppHomePageHandlerTest(const AppHomePageHandlerTest&) = delete;
  AppHomePageHandlerTest& operator=(const AppHomePageHandlerTest&) = delete;

  ~AppHomePageHandlerTest() override = default;

 protected:
  std::unique_ptr<TestAppHomePageHandler> GetAppHomePageHandler() {
    AddBlankTabAndShow(browser());
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    test_web_ui_.set_web_contents(contents);

    return std::make_unique<TestAppHomePageHandler>(&test_web_ui_, profile(),
                                                    page_.BindAndGetRemote());
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  AppId InstallTestWebApp() {
    AppId installed_app_id =
        web_app::test::InstallWebApp(profile(), BuildWebAppInfo());

    return installed_app_id;
  }

  Profile* profile() { return browser()->profile(); }

  void UninstallTestWebApp(const web_app::AppId& app_id) {
    web_app::test::UninstallWebApp(profile(), app_id);
  }

  scoped_refptr<const extensions::Extension> InstallTestExtensionApp() {
    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName, kTestAppName);
    manifest.SetString(extensions::manifest_keys::kVersion, "0.0.0.0");
    manifest.SetString(extensions::manifest_keys::kApp, "true");
    manifest.SetString(extensions::manifest_keys::kPlatformAppBackgroundPage,
                       std::string());

    std::string error;
    scoped_refptr<extensions::Extension> extension =
        extensions::Extension::Create(
            base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
            manifest.GetDict(), 0, &error);

    extension_service()->AddExtension(extension.get());
    return extension;
  }

  void UninstallTestExtensionApp(const extensions::Extension* extension) {
    std::u16string error;
    base::RunLoop run_loop;

    // `UninstallExtension` method synchronously removes the extension from the
    // set of installed extensions stored in the ExtensionRegistry and later
    // notifies interested observer of extension uninstall event. But it will
    // asynchronously remove site-related data and the files stored on disk.
    // It's common case that `WebappTest::TearDonw` invokes before
    // `ExtensionService` completes delete related file, as a result, the
    // `AppHome` test would finally fail delete testing-related file for file
    // locking semantics on WinOS platfom. To workaround this case, make sure
    // the task of uninstalling extension complete before the `AppHome` test
    // tear down.
    extension_service()->UninstallExtension(
        extension->id(),
        extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING, &error,
        base::BindOnce(
            [](base::OnceClosure quit_closure) {
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  extensions::ExtensionService* CreateTestExtensionService() {
    auto* extension_system = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile()));
    extensions::ExtensionService* ext_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ext_service->Init();
    return ext_service;
  }

  content::TestWebUI test_web_ui_;
  testing::StrictMock<MockAppHomePage> page_;
#if BUILDFLAG(IS_WIN)
  // This prevents SetRunOnOsLoginMode from leaving shortcuts in the Windows
  // startup directory that cause Chrome to get launched when Windows starts on
  // a bot. It needs to be in the class so that the override lasts until the
  // test object is destroyed, because tasks can keep running after the test
  // method finishes.
  // See https://crbug.com/1239809
  base::ScopedPathOverride override_user_startup_{base::DIR_USER_STARTUP};
#endif  // BUILDFLAG(IS_WIN)
};

MATCHER_P(MatchAppName, expected_app_name, "") {
  if (expected_app_name == arg->name) {
    return true;
  }
  return false;
}

MATCHER_P(MatchAppId, expected_app_id, "") {
  if (expected_app_id == arg->id) {
    return true;
  }
  return false;
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, GetApps) {
  AppId installed_app_id = InstallTestWebApp();

  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  std::vector<app_home::mojom::AppInfoPtr> app_infos;
  base::RunLoop run_loop;
  page_handler->GetApps(
      WrapGetAppsCallback(&app_infos, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(kTestAppUrl, app_infos[0]->start_url);
  EXPECT_EQ(kTestAppName, app_infos[0]->name);
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnWebAppInstalled) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnExtensionLoaded) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  ASSERT_NE(extension, nullptr);
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnWebAppUninstall) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a web app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

  // Check uninstall previous web app will call `RemoveApp` API.
  EXPECT_CALL(page_, RemoveApp(MatchAppId(installed_app_id)))
      .Times(testing::AtLeast(1));
  UninstallTestWebApp(installed_app_id);
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnExtensionUninstall) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test extension app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  page_handler->Wait();

  // Check uninstall previous extension will call `RemoveApp` API.
  EXPECT_CALL(page_, RemoveApp(MatchAppId(extension->id())))
      .Times(testing::AtLeast(1));
  UninstallTestExtensionApp(extension.get());
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, UninstallWebApp) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test web app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

  // Then, check uninstalling previous web app via using
  // `AppHomePageHandler::UninstallApp`.
  EXPECT_CALL(page_, RemoveApp(MatchAppId(installed_app_id)))
      .Times(testing::AtLeast(1));
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  page_handler->UninstallApp(installed_app_id);
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, UninstallExtensionApp) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test extension app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  page_handler->Wait();

  // Then, check uninstalling previous extension app via using
  // `AppHomePageHandler::UninstallApp`.
  EXPECT_CALL(page_, RemoveApp(MatchAppId(extension->id())))
      .Times(testing::AtLeast(1));
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  page_handler->UninstallApp(extension->id());
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, ShowWebAppSettings) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test web app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

  content::WebContentsAddedObserver nav_observer;
  page_handler->ShowAppSettings(installed_app_id);
  // Wait for new web content to be created.
  nav_observer.GetWebContents();
  GURL url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(url, GURL(chrome::kChromeUIWebAppSettingsURL + installed_app_id));
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, CreateWebAppShortcut) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test web app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

#if BUILDFLAG(IS_MAC)
  base::RunLoop loop;
  page_handler->CreateAppShortcut(installed_app_id, loop.QuitClosure());
  loop.Run();
#else
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "CreateChromeApplicationShortcutView");
  page_handler->CreateAppShortcut(installed_app_id, base::DoNothing());
  FlushShortcutTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget != nullptr);
  views::test::AcceptDialog(widget);
#endif
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, CreateExtensionAppShortcut) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test extension app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  page_handler->Wait();

#if BUILDFLAG(IS_MAC)
  base::RunLoop loop;
  page_handler->CreateAppShortcut(extension->id(), loop.QuitClosure());
  loop.Run();
#else
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "CreateChromeApplicationShortcutView");
  page_handler->CreateAppShortcut(extension->id(), base::DoNothing());
  FlushShortcutTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget != nullptr);
  views::test::AcceptDialog(widget);
#endif
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, SetRunOnOsLoginMode) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

  page_handler->SetRunOnOsLoginMode(installed_app_id,
                                    web_app::RunOnOsLoginMode::kWindowed);
  base::RunLoop loop;
  page_handler->WaitForRunOnOsLoginModeChanged(loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(web_app::RunOnOsLoginMode::kWindowed,
            web_app::WebAppProvider::GetForWebApps(profile())
                ->registrar()
                .GetAppRunOnOsLoginMode(installed_app_id)
                .value);
}

}  // namespace webapps
