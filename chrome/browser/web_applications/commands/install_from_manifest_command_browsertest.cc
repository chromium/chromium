// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/install_from_manifest_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

class InstallFromManifestCommandTest : public WebAppControllerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    os_hooks_suppress_.reset();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverride::OverrideForTesting(base::GetHomeDir());
    }
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(test::UninstallAllWebApps(profile()));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppControllerBrowserTest::TearDownOnMainThread();
  }

  bool IsShortcutCreated(const AppId& app_id, const std::string& name) {
#if BUILDFLAG(IS_CHROMEOS)
    // Shortcuts are always created (through App Service) on ChromeOS.
    return true;
#else
    base::ScopedAllowBlockingForTesting allow_blocking;
    return test_override_->test_override->IsShortcutCreated(profile(), app_id,
                                                            name);
#endif
  }

 private:
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      test_override_;
};

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, SuccessNewInstall) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kStartUrl("https://www.app.com/home");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = R"json({
    "start_url": "/home",
    "name": "Test app",
    "launch_handler": {
      "client_mode": "focus-existing"
    }
  })json";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id, GenerateAppId(/*manifest_id=*/absl::nullopt, kStartUrl));
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(result_id),
            "Test app");
  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppById(result_id)
                ->launch_handler()
                ->client_mode,
            blink::Manifest::LaunchHandler::ClientMode::kFocusExisting);
  EXPECT_TRUE(IsShortcutCreated(result_id, "Test app"));
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest,
                       SuccessCrossOriginManifest) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.cdn.com/app/manifest.json");
  const char kManifest[] = R"json({
    "start_url": "https://www.app.com/",
    "name": "Test app"
  })json";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, kDocumentUrl));
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));
  EXPECT_TRUE(IsShortcutCreated(result_id, "Test app"));
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, SuccessWithManifestId) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = R"json({
    "start_url": "/",
    "id": "/appid",
    "name": "Test app"
  })json";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id, GenerateAppId("appid", kDocumentUrl));
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));
  EXPECT_TRUE(IsShortcutCreated(result_id, "Test app"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(result_id),
            "Test app");
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, SuccessWithExistingApp) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = R"json({
    "start_url": "/",
    "name": "Manifest installed app"
  })json";

  AppId existing_id =
      test::InstallDummyWebApp(profile(), "User installed app", kDocumentUrl);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::INTERNAL_DEFAULT, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id, existing_id);
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));

  // Existing install data should not be overwritten.
  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(result_id),
            "User installed app");
  /// Existing install should be updated with the new install source.
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(result_id)->IsPreinstalledApp());
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, FailureInvalidManifest) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = "notjson";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kNotValidManifestForWebApp);
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, FailureInvalidStartUrl) {
  // Installation will fail because there's no valid start URL.
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = R"json({
    "name": "Test app 2"
  })json";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kNotValidManifestForWebApp);
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest,
                       FailureStartUrlOriginMismatch) {
  // Installation will fail because the start URL is a different origin to the
  // document URL.
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = R"json({
    "start_url": "https://www.not-app.com/",
    "name": "Test app 2"
  })json";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest, result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kNotValidManifestForWebApp);
}

}  // namespace web_app
