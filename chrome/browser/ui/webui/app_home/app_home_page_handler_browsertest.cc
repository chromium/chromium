// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/create_application_shortcut_view_test_support.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/mock_app_home_page.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

using webapps::AppId;
using GetAppsCallback =
    base::OnceCallback<void(std::vector<app_home::mojom::AppInfoPtr>)>;

namespace webapps {

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestAppName[] = "Test App";
constexpr char kTestAppNameWithUnsupportedText[] = "Test App (unsupported app)";

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
    // TODO(crbug.com/40234138): Define specific Wait for each
    // listener.
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForRunOnOsLoginModeChanged(base::OnceClosure handle) {
    run_on_os_login_mode_changed_handle_ = std::move(handle);
  }

 private:
  void OnWebAppInstalled(const webapps::AppId& app_id) override {
    run_loop_->Quit();
    AppHomePageHandler::OnWebAppInstalled(app_id);
  }

  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override {
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
      const webapps::AppId& app_id,
      web_app::RunOnOsLoginMode run_on_os_login_mode) override {
    std::move(run_on_os_login_mode_changed_handle_).Run();
    AppHomePageHandler::OnWebAppRunOnOsLoginModeChanged(app_id,
                                                        run_on_os_login_mode);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure run_on_os_login_mode_changed_handle_;
};

std::unique_ptr<web_app::WebAppInstallInfo> BuildWebAppInfo(
    std::string test_app_name) {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kTestAppUrl));
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = base::UTF8ToUTF16(std::string_view(test_app_name));
  app_info->manifest_url = GURL(kTestManifestUrl);

  return app_info;
}

}  // namespace

class AppHomePageHandlerTest : public InProcessBrowserTest {
 public:
  AppHomePageHandlerTest() = default;

  AppHomePageHandlerTest(const AppHomePageHandlerTest&) = delete;
  AppHomePageHandlerTest& operator=(const AppHomePageHandlerTest&) = delete;

  ~AppHomePageHandlerTest() override = default;

  void SetUpOnMainThread() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
    web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(profile());
    base::ScopedAllowBlockingForTesting allow_blocking;
    override_registration_.reset();
  }

 protected:
  std::unique_ptr<TestAppHomePageHandler> GetAppHomePageHandler() {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    test_web_ui_.set_web_contents(contents);

    return std::make_unique<TestAppHomePageHandler>(&test_web_ui_, profile(),
                                                    page_.BindAndGetRemote());
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  webapps::AppId InstallTestWebApp(
      WebappInstallSource install_source =
          WebappInstallSource::OMNIBOX_INSTALL_ICON,
      std::string test_app_name = kTestAppName) {
    webapps::AppId installed_app_id = web_app::test::InstallWebApp(
        profile(), BuildWebAppInfo(test_app_name),
        /*overwrite_existing_manifest_fields=*/false, install_source);

    return installed_app_id;
  }

  Profile* profile() { return browser()->profile(); }

  void UninstallTestWebApp(const webapps::AppId& app_id) {
    web_app::test::UninstallWebApp(profile(), app_id);
  }

  scoped_refptr<const extensions::Extension> InstallTestExtensionApp() {
    base::Value::Dict manifest;
    manifest.SetByDottedPath(extensions::manifest_keys::kName, kTestAppName);
    manifest.SetByDottedPath(extensions::manifest_keys::kVersion, "0.0.0.0");
    manifest.SetByDottedPath(
        extensions::manifest_keys::kPlatformAppBackgroundPage, std::string());

    std::string error;
    scoped_refptr<extensions::Extension> extension =
        extensions::Extension::Create(
            base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
            manifest, 0, &error);

    extension_service()->AddExtension(extension.get());
    return extension;
  }

  scoped_refptr<const extensions::Extension> InstallTestExtension() {
    namespace keys = extensions::manifest_keys;
    base::Value::Dict manifest = base::Value::Dict()
                                     .Set(keys::kName, "Test extension")
                                     .Set(keys::kVersion, "1.0")
                                     .Set(keys::kManifestVersion, 2);

    std::string error;
    scoped_refptr<extensions::Extension> extension =
        extensions::Extension::Create(
            base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
            manifest, 0, &error);

    extension_service()->AddExtension(extension.get());
    return extension;
  }

  void UninstallTestExtension(const extensions::Extension* extension) {
    std::u16string error;
    base::RunLoop run_loop;

    // `UninstallExtension` method synchronously removes the extension from the
    // set of installed extensions stored in the ExtensionRegistry and later
    // notifies interested observer of extension uninstall event. But it will
    // asynchronously remove site-related data and the files stored on disk.
    // It's common case that `WebappTest::TearDown` invokes before
    // `ExtensionService` completes delete related file, as a result, the
    // `AppHome` test would finally fail delete testing-related file for file
    // locking semantics on WinOS platfom. To workaround this case, make sure
    // the task of uninstalling extension complete before the `AppHome` test
    // tear down.
    extension_service()->UninstallExtension(
        extension->id(),
        extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING, &error,
        run_loop.QuitClosure());
    run_loop.Run();
  }

  content::TestWebUI test_web_ui_;
  testing::StrictMock<MockAppHomePage> page_;

  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
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
  webapps::AppId installed_app_id = InstallTestWebApp();

  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  base::test::TestFuture<std::vector<app_home::mojom::AppInfoPtr>> future;
  page_handler->GetApps(future.GetCallback());
  auto app_infos = future.Take();

  EXPECT_EQ(kTestAppUrl, app_infos[0]->start_url);
  EXPECT_EQ(kTestAppName, app_infos[0]->name);
  EXPECT_TRUE(app_infos[0]->may_uninstall);
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, ForceInstalledApp) {
  webapps::AppId installed_app_id =
      InstallTestWebApp(WebappInstallSource::EXTERNAL_POLICY);

  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  base::test::TestFuture<std::vector<app_home::mojom::AppInfoPtr>> future;
  page_handler->GetApps(future.GetCallback());
  auto app_infos = future.Take();

  EXPECT_FALSE(app_infos[0]->may_uninstall);
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnWebAppInstalled) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  webapps::AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnExtensionLoaded_App) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppNameWithUnsupportedText)))
      .Times(testing::AtLeast(1));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  ASSERT_NE(extension, nullptr);
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnExtensionLoaded_Extension) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppNameWithUnsupportedText)))
      .Times(0);
  scoped_refptr<const extensions::Extension> extension = InstallTestExtension();
  ASSERT_NE(extension, nullptr);
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, OnWebAppUninstall) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a web app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  webapps::AppId installed_app_id = InstallTestWebApp();
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
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppNameWithUnsupportedText)))
      .Times(testing::AtLeast(1));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  page_handler->Wait();

  // Check uninstall previous extension will call `RemoveApp` API.
  EXPECT_CALL(page_, RemoveApp(MatchAppId(extension->id())))
      .Times(testing::AtLeast(1));
  UninstallTestExtension(extension.get());
  page_handler->Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, UninstallWebApp) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test web app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  webapps::AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

  // Then, check uninstalling previous web app via using
  // `AppHomePageHandler::UninstallApp`.
  EXPECT_CALL(page_, RemoveApp(MatchAppId(installed_app_id)))
      .Times(testing::AtLeast(1));
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  web_app::WebAppTestUninstallObserver observer(profile());
  observer.BeginListening({installed_app_id});
  page_handler->UninstallApp(installed_app_id);
  page_handler->Wait();
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, UninstallExtensionApp) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test extension app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppNameWithUnsupportedText)));
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
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  webapps::AppId installed_app_id = InstallTestWebApp();
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
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  webapps::AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

#if BUILDFLAG(IS_MAC)
  base::RunLoop loop;
  page_handler->CreateAppShortcut(installed_app_id, loop.QuitClosure());
  loop.Run();
#else
  CreateChromeApplicationShortcutViewWaiter waiter;
  page_handler->CreateAppShortcut(installed_app_id, base::DoNothing());
  FlushShortcutTasks();
  std::move(waiter).WaitForAndAccept();
  FlushShortcutTasks();
  web_app::WebAppProvider::GetForTest(profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();
#endif
  EXPECT_CALL(page_, RemoveApp(MatchAppId(installed_app_id)))
      .Times(testing::AtLeast(1));
  UninstallTestWebApp(installed_app_id);
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, CreateExtensionAppShortcut) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();

  // First, install a test extension app for test.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppNameWithUnsupportedText)))
      .Times(testing::AtLeast(1));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  page_handler->Wait();

#if BUILDFLAG(IS_MAC)
  base::RunLoop loop;
  page_handler->CreateAppShortcut(extension->id(), loop.QuitClosure());
  loop.Run();
#else
  CreateChromeApplicationShortcutViewWaiter waiter;
  page_handler->CreateAppShortcut(extension->id(), base::DoNothing());
  FlushShortcutTasks();
  std::move(waiter).WaitForAndAccept();
#endif
  EXPECT_CALL(page_, RemoveApp(MatchAppId(extension->id())))
      .Times(testing::AtLeast(1));
  UninstallTestExtension(extension.get());
#if !BUILDFLAG(IS_MAC)
  FlushShortcutTasks();
#endif
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, SetRunOnOsLoginMode) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  // Add happens twice, on install & on os integration complete.
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)))
      .Times(testing::AtLeast(1));
  webapps::AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();

  page_handler->SetRunOnOsLoginMode(installed_app_id,
                                    web_app::RunOnOsLoginMode::kWindowed);
  base::RunLoop loop;
  page_handler->WaitForRunOnOsLoginModeChanged(loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(web_app::RunOnOsLoginMode::kWindowed,
            web_app::WebAppProvider::GetForWebApps(profile())
                ->registrar_unsafe()
                .GetAppRunOnOsLoginMode(installed_app_id)
                .value);
}

IN_PROC_BROWSER_TEST_F(AppHomePageHandlerTest, HandleLaunchDeprecatedApp) {
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler();
  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppNameWithUnsupportedText)))
      .Times(testing::AtLeast(1));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  page_handler->Wait();

  auto waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{}, "DeprecatedAppsDialogView");
  page_handler->LaunchApp(extension->id(), nullptr);
  // Launch deprecated app will show deprecated apps dialog view.
  EXPECT_NE(waiter.WaitIfNeededAndGet(), nullptr);
}

}  // namespace webapps
