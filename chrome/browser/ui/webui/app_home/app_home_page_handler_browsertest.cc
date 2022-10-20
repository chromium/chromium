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
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

using web_app::AppId;
using GetAppsCallback =
    base::OnceCallback<void(std::vector<app_home::mojom::AppInfoPtr>)>;

namespace webapps {

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestAppName[] = "Test App";

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
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
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

  std::unique_ptr<base::RunLoop> run_loop_;
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
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(kTestAppName).Build();
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

}  // namespace webapps
