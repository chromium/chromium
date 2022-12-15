// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using OsIntegrationSubManagersState = web_app::OsIntegrationSubManagersState;
using AppId = web_app::AppId;
using WebAppProvider = web_app::WebAppProvider;

namespace {

using ::testing::Optional;

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kMethodNameAppAdded[] = "ntp.appAdded";
constexpr char kKeyAppId[] = "id";
constexpr char kKeyIsLocallyInstalled[] = "isLocallyInstalled";
const std::u16string kTestAppTitle = u"Test App";

class TestAppLauncherHandler : public AppLauncherHandler {
 public:
  TestAppLauncherHandler(extensions::ExtensionService* extension_service,
                         WebAppProvider* provider,
                         content::TestWebUI* test_web_ui)
      : AppLauncherHandler(extension_service, provider) {
    DCHECK(test_web_ui->GetWebContents());
    DCHECK(test_web_ui->GetWebContents()->GetBrowserContext());
    set_web_ui(test_web_ui);
  }

  TestAppLauncherHandler(const TestAppLauncherHandler&) = delete;
  TestAppLauncherHandler& operator=(const TestAppLauncherHandler&) = delete;

  ~TestAppLauncherHandler() override = default;

  content::TestWebUI* test_web_ui() {
    return static_cast<content::TestWebUI*>(web_ui());
  }

  using CallData = content::TestWebUI::CallData;
  const std::vector<std::unique_ptr<CallData>>& call_data() {
    return test_web_ui()->call_data();
  }
};

std::unique_ptr<WebAppInstallInfo> BuildWebAppInfo() {
  auto app_info = std::make_unique<WebAppInstallInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);

  return app_info;
}

}  // namespace

class AppLauncherHandlerTest
    : public BrowserWithTestWindowTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  AppLauncherHandlerTest() {
    if (GetParam() == OsIntegrationSubManagersState::kEnabled) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers, {{"stage", "write_config"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{}, {features::kOsIntegrationSubManagers});
    }
  }

  AppLauncherHandlerTest(const AppLauncherHandlerTest&) = delete;
  AppLauncherHandlerTest& operator=(const AppLauncherHandlerTest&) = delete;

  ~AppLauncherHandlerTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    extension_service_ = CreateTestExtensionService();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  std::unique_ptr<TestAppLauncherHandler> GetAppLauncherHandler(
      content::TestWebUI* test_web_ui) {
    return std::make_unique<TestAppLauncherHandler>(
        extension_service_, WebAppProvider::GetForTest(profile()), test_web_ui);
  }

  // Install a web app and sets the locally installed property based on
  // |is_locally_installed|.
  AppId InstallWebApp(bool is_locally_installed = true) {
    AppId installed_app_id =
        web_app::test::InstallWebApp(profile(), BuildWebAppInfo());
    if (is_locally_installed)
      return installed_app_id;

    auto* web_app_provider = WebAppProvider::GetForTest(profile());
    web_app_provider->sync_bridge_unsafe().SetAppIsLocallyInstalled(
        installed_app_id, false);
    web_app_provider->sync_bridge_unsafe().SetAppInstallTime(installed_app_id,
                                                             base::Time::Min());
    return installed_app_id;
  }

  // Validates the expectations for the JS call made after locally installing a
  // web app.
  void ValidateLocallyInstalledCallData(
      TestAppLauncherHandler* app_launcher_handler,
      const AppId& installed_app_id) {
    ASSERT_EQ(1U, app_launcher_handler->call_data().size());
    EXPECT_EQ(kMethodNameAppAdded,
              app_launcher_handler->call_data()[0]->function_name());

    const base::Value* arg1 = app_launcher_handler->call_data()[0]->arg1();

    ASSERT_TRUE(arg1->is_dict());
    const base::Value::Dict& app_info = arg1->GetDict();

    const std::string* app_id = app_info.FindString(kKeyAppId);
    ASSERT_TRUE(app_id);
    EXPECT_EQ(*app_id, installed_app_id);

    EXPECT_THAT(app_info.FindBoolByDottedPath(kKeyIsLocallyInstalled),
                Optional(true));
  }

  std::unique_ptr<content::TestWebUI> CreateTestWebUI(
      content::WebContents* test_web_contents) {
    auto test_web_ui = std::make_unique<content::TestWebUI>();
    test_web_ui->set_web_contents(test_web_contents);

    return test_web_ui;
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    auto site_instance = content::SiteInstance::Create(profile());
    return content::WebContentsTester::CreateTestWebContents(
        profile(), std::move(site_instance));
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

  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  raw_ptr<extensions::ExtensionService> extension_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that AppLauncherHandler::HandleInstallAppLocally calls the JS method
// "ntp.appAdded" for the locally installed app.
TEST_P(AppLauncherHandlerTest, HandleInstallAppLocally) {
  AppId installed_app_id = InstallWebApp(/*is_locally_installed=*/false);

  // Initialize the web_ui instance.
  std::unique_ptr<content::WebContents> test_web_contents =
      CreateTestWebContents();
  std::unique_ptr<content::TestWebUI> test_web_ui =
      CreateTestWebUI(test_web_contents.get());
  std::unique_ptr<TestAppLauncherHandler> app_launcher_handler =
      GetAppLauncherHandler(test_web_ui.get());

  base::Value::List args;
  args.Append(base::Value(installed_app_id));
  app_launcher_handler->HandleGetApps(/*args=*/base::Value::List());
  app_launcher_handler->test_web_ui()->ClearTrackedCalls();

  // Call AppLauncherHandler::HandleInstallAppLocally for the web_ui and expect
  // that the JS is made correctly.
  app_launcher_handler->HandleInstallAppLocally(args);

  ValidateLocallyInstalledCallData(app_launcher_handler.get(),
                                   installed_app_id);
}

// Tests that AppLauncherHandler::HandleInstallAppLocally calls the JS method
// "ntp.appAdded" for the all the running instances of chrome://apps page.
TEST_P(AppLauncherHandlerTest, HandleInstallAppLocally_MultipleWebUI) {
  AppId installed_app_id = InstallWebApp(/*is_locally_installed=*/false);

  // Initialize the first web_ui instance.
  std::unique_ptr<content::WebContents> test_web_contents_1 =
      CreateTestWebContents();
  std::unique_ptr<content::TestWebUI> test_web_ui_1 =
      CreateTestWebUI(test_web_contents_1.get());
  std::unique_ptr<TestAppLauncherHandler> app_launcher_handler_1 =
      GetAppLauncherHandler(test_web_ui_1.get());

  base::Value::List args;
  args.Append(base::Value(installed_app_id));
  app_launcher_handler_1->HandleGetApps(/*args=*/base::Value::List());
  app_launcher_handler_1->test_web_ui()->ClearTrackedCalls();

  // Initialize the second web_ui instance.
  std::unique_ptr<content::WebContents> test_web_contents_2 =
      CreateTestWebContents();
  std::unique_ptr<content::TestWebUI> test_web_ui_2 =
      CreateTestWebUI(test_web_contents_2.get());
  std::unique_ptr<TestAppLauncherHandler> app_launcher_handler_2 =
      GetAppLauncherHandler(test_web_ui_2.get());
  app_launcher_handler_2->HandleGetApps(/*args=*/base::Value::List());
  app_launcher_handler_2->test_web_ui()->ClearTrackedCalls();

  // Call AppLauncherHandler::HandleInstallAppLocally for the first web_ui
  // handler and expect the correct JS call is made to both the web_ui
  // instances.
  app_launcher_handler_1->HandleInstallAppLocally(args);

  ValidateLocallyInstalledCallData(app_launcher_handler_1.get(),
                                   installed_app_id);
  ValidateLocallyInstalledCallData(app_launcher_handler_2.get(),
                                   installed_app_id);
}

// Regression test for crbug.com/1302157.
TEST_P(AppLauncherHandlerTest, HandleClosedWhileUninstallingExtension) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("foo").Build();
  extension_service_->AddExtension(extension.get());

  AddTab(browser(), GURL("http://foo/1"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  std::unique_ptr<content::WebContents> test_web_contents =
      CreateTestWebContents();
  std::unique_ptr<content::TestWebUI> test_web_ui = CreateTestWebUI(contents);
  std::unique_ptr<TestAppLauncherHandler> app_launcher_handler =
      GetAppLauncherHandler(test_web_ui.get());

  app_launcher_handler->CreateExtensionUninstallDialog()->ConfirmUninstall(
      extension, extensions::UNINSTALL_REASON_USER_INITIATED,
      extensions::UNINSTALL_SOURCE_CHROME_APPS_PAGE);
  app_launcher_handler.reset();
  // No crash (in asan tester) indicates a passing score.
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AppLauncherHandlerTest,
    ::testing::Values(OsIntegrationSubManagersState::kEnabled,
                      OsIntegrationSubManagersState::kDisabled),
    web_app::test::GetOsIntegrationSubManagersTestName);
