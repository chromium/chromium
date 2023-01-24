// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/install_from_manifest_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {

class InstallFromManifestCommandTest : public WebAppControllerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();

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

  std::string GetIconUrl() {
    return https_server()->GetURL("/web_apps/blue-192.png").spec();
  }

  base::flat_set<std::string> GetHostAllowlist() { return {"127.0.0.1"}; }

 private:
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      test_override_;
};

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, SuccessNewInstall) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kStartUrl("https://www.app.com/home");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifestTemplate[] = R"json({
    "start_url": "/home",
    "name": "Test app",
    "launch_handler": {
      "client_mode": "focus-existing"
    },
    "icons": [{
      "src": "$1",
      "sizes": "192x192",
      "type": "image/png"
    }]
  })json";

  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate, {GetIconUrl()}, nullptr);

  AppId expected_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kStartUrl);
  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, manifest, expected_id, GetHostAllowlist(),
          result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id, expected_id);
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(result_id),
            "Test app");
  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppById(result_id)
                ->launch_handler()
                ->client_mode,
            blink::Manifest::LaunchHandler::ClientMode::kFocusExisting);
  SkColor icon_color =
      IconManagerReadAppIconPixel(provider().icon_manager(), result_id, 96);
  EXPECT_EQ(icon_color, SK_ColorBLUE);

  EXPECT_TRUE(IsShortcutCreated(result_id, "Test app"));
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest,
                       SuccessCrossOriginManifest) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.cdn.com/app/manifest.json");
  const char kManifestTemplate[] = R"json({
    "start_url": "https://www.app.com/",
    "name": "Test app",
    "icons": [{
      "src": "$1",
      "sizes": "192x192",
      "type": "image/png"
    }]
  })json";

  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate, {GetIconUrl()}, nullptr);

  AppId expected_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kDocumentUrl);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, manifest, expected_id, GetHostAllowlist(),
          result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id, expected_id);
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));
  EXPECT_TRUE(IsShortcutCreated(result_id, "Test app"));
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, SuccessWithManifestId) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifestTemplate[] = R"json({
    "start_url": "/",
    "id": "/appid",
    "name": "Test app",
    "icons": [{
      "src": "$1",
      "sizes": "192x192",
      "type": "image/png"
    }]
  })json";

  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate, {GetIconUrl()}, nullptr);

  AppId expected_id = GenerateAppId("appid", kDocumentUrl);
  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, manifest, expected_id, GetHostAllowlist(),
          result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_EQ(result_id, expected_id);
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));
  EXPECT_TRUE(IsShortcutCreated(result_id, "Test app"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(result_id),
            "Test app");
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, SuccessWithExistingApp) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifestTemplate[] = R"json({
    "start_url": "/",
    "name": "Manifest installed app",
    "icons": [{
      "src": "$1",
      "sizes": "192x192",
      "type": "image/png"
    }]
  })json";
  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate, {GetIconUrl()}, nullptr);

  AppId existing_id =
      test::InstallDummyWebApp(profile(), "User installed app", kDocumentUrl);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::INTERNAL_DEFAULT, kDocumentUrl,
          kManifestUrl, manifest, existing_id, GetHostAllowlist(),
          result.GetCallback()));

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

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest,
                       SuccessWithMultipleIcons) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifestTemplate[] = R"json({
    "start_url": "/ ",
    "name": "Test app",
    "icons": [
      {
        "src": "$1",
        "sizes": "96x96",
        "type": "image/png"
      },
      {
        "src": "$2",
        "sizes": "192x192",
        "type": "image/png"
      }
    ],
    "shortcuts": [
      {
        "name": "Shortcut",
        "url": "/shortcut",
        "icons": [{
          "src": "$3",
          "sizes": "192x192",
          "type": "image/png"
        }]
      }
    ]
  })json";

  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate,
      {https_server()->GetURL("/banners/96x96-red.png").spec(),
       https_server()->GetURL("/banners/192x192-green.png").spec(),
       https_server()->GetURL("/web_apps/blue-192.png").spec()

      },
      nullptr);
  AppId expected_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kDocumentUrl);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::INTERNAL_DEFAULT, kDocumentUrl,
          kManifestUrl, manifest, expected_id, GetHostAllowlist(),
          result.GetCallback()));

  AppId result_id = result.Get<0>();
  EXPECT_TRUE(webapps::IsSuccess(result.Get<1>()));

  SkColor small_icon_color =
      IconManagerReadAppIconPixel(provider().icon_manager(), result_id, 96);
  EXPECT_EQ(small_icon_color, SK_ColorRED);

  SkColor large_icon_color =
      IconManagerReadAppIconPixel(provider().icon_manager(), result_id, 192);
  EXPECT_EQ(large_icon_color, SK_ColorGREEN);

  base::test::TestFuture<ShortcutsMenuIconBitmaps> shortcut_future;
  provider().icon_manager().ReadAllShortcutsMenuIcons(
      result_id, shortcut_future.GetCallback());

  SkColor shortcut_icon_color =
      shortcut_future.Get()[0].any.at(192).getColor(0, 0);
  EXPECT_EQ(shortcut_icon_color, SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, FailureInvalidManifest) {
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const char kManifest[] = "notjson";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest,
          /*expected_id=*/"", GetHostAllowlist(), result.GetCallback()));

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
          kManifestUrl, kManifest,
          /*expected_id=*/"", GetHostAllowlist(), result.GetCallback()));

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
    "name": "Test app 2",
    "icons": [{
      "src": "/icon/96.png",
      "sizes": "96x96",
      "type": "image/png"
    }]
  })json";

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, kManifest,
          /*expected_id=*/"", GetHostAllowlist(), result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kNotValidManifestForWebApp);
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest,
                       FailureExpectedIdMismatch) {
  // Installation will fail because the expected ID doesn't match the ID in the
  // manifest.
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const AppId kExpectedId = GenerateAppId("/two", kDocumentUrl);
  const char kManifestTemplate[] = R"json({
    "start_url": "/",
    "id": "/one",
    "name": "Test app 2",
    "icons": [{
      "src": "$1",
      "sizes": "192x192",
      "type": "image/png"
    }]
  })json";
  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate, {GetIconUrl()}, nullptr);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, manifest, kExpectedId, GetHostAllowlist(),
          result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kExpectedAppIdCheckFailed);
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, FailureNoValidIcons) {
  // Installation will fail because there are no icons that match the allowlist.
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const AppId kExpectedId = GenerateAppId("/two", kDocumentUrl);
  const char kManifestTemplate[] = R"json({
    "start_url": "/",
    "name": "Test app",
    "icons": [
      {
        "src": "/icons/96.png",
        "sizes": "96x96",
        "type": "image/png"
      },
      {
        "src": "https://www.somesite.com/icons/48.png",
        "sizes": "48x48",
        "type": "image/png"
      },
      {
        "src": "https://notcdn.com/icons/192.png",
        "sizes": "192x192",
        "type": "image/png"
      },
    ]
  })json";
  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate, {GetIconUrl()}, nullptr);
  base::flat_set<std::string> host_allowlist = {"cdn.com", "subdomain.app.com"};

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, manifest, kExpectedId, host_allowlist,
          result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kNotValidManifestForWebApp);
}

IN_PROC_BROWSER_TEST_F(InstallFromManifestCommandTest, FailureAllIconsError) {
  // Installation will fail because all icons error when downloaded.
  const GURL kDocumentUrl("https://www.app.com/");
  const GURL kManifestUrl("https://www.app.com/manifest.json");
  const AppId kExpectedId = GenerateAppId("/two", kDocumentUrl);
  const char kManifestTemplate[] = R"json({
    "start_url": "/",
    "id": "/one",
    "name": "Test app 2",
    "icons": [
      {
        "src": "$1",
        "sizes": "96x96",
        "type": "image/png"
      },
      {
        "src": "$2",
        "sizes": "48x48",
        "type": "image/png"
      }
    ]
  })json";
  std::string manifest = base::ReplaceStringPlaceholders(
      kManifestTemplate,
      {https_server()->GetURL("/404").spec(),
       https_server()->GetURL("/nocontent").spec()},
      nullptr);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallFromManifestCommand>(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, kDocumentUrl,
          kManifestUrl, manifest, kExpectedId, GetHostAllowlist(),
          result.GetCallback()));

  EXPECT_EQ(result.Get<1>(),
            webapps::InstallResultCode::kIconDownloadingFailed);
}

}  // namespace web_app
