// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include <memory>
#include <optional>
#include <ostream>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_observer_test_utils.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace apps {
// Required for gmock matchers.
void PrintTo(const IconInfo& info, std::ostream* os) {
  *os << info.AsDebugValue().DebugString();
}
}  // namespace apps

namespace web_app {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;

class InstallFromSyncTest : public WebAppTest {
 public:
  const int kIconSize = 96;
  const GURL kWebAppStartUrl = GURL("https://example.com/path/index.html");
  const webapps::ManifestId kWebAppManifestId =
      GURL("https://example.com/path/index.html");

  const GURL kOtherWebAppStartUrl =
      GURL("https://example.com/path2/index.html");
  const webapps::ManifestId kOtherWebAppManifestId =
      GURL("https://example.com/path2/index.html");

  const std::u16string kManifestName = u"Manifest Name";
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");
  const GURL kManifestIconUrl =
      GURL("https://example.com/path/manifest_icon.png");
  const SkColor kManifestIconColor = SK_ColorCYAN;

  const std::string kFallbackTitle = "Fallback Title";
  const GURL kFallbackIconUrl =
      GURL("https://example.com/path/fallback_icon.png");
  const SkColor kFallbackIconColor = SK_ColorBLUE;

  const std::u16string kDocumentTitle = u"Document Title";
  const GURL kDocumentIconUrl =
      GURL("https://example.com/path/document_icon.png");
  const SkColor kDocumentIconColor = SK_ColorRED;

  InstallFromSyncTest() = default;
  ~InstallFromSyncTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    provider()->Shutdown();
    WebAppTest::TearDown();
  }

 protected:
  struct InstallResult {
    bool callback_triggered = false;
    webapps::AppId installed_app_id;
    webapps::InstallResultCode install_code;
    std::optional<webapps::InstallResultCode> install_code_before_fallback;
  };

  InstallFromSyncCommand::Params CreateParams(webapps::AppId app_id,
                                              webapps::ManifestId manifest_id,
                                              GURL start_url) {
    return InstallFromSyncCommand::Params(
        app_id, manifest_id, start_url, kFallbackTitle,
        start_url.GetWithoutFilename(), /*theme_color=*/std::nullopt,
        mojom::UserDisplayMode::kStandalone, /*icons=*/
        {apps::IconInfo(kFallbackIconUrl, kIconSize)});
  }

  InstallResult InstallFromSyncAndWait(GURL start_url,
                                       webapps::ManifestId manifest_id) {
    const webapps::AppId app_id = GenerateAppIdFromManifestId(manifest_id);
    InstallResult result;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        future;
    std::unique_ptr<InstallFromSyncCommand> command =
        std::make_unique<InstallFromSyncCommand>(
            profile(), CreateParams(app_id, manifest_id, start_url),
            future.GetCallback());
    command->SetFallbackTriggeredForTesting(
        base::BindLambdaForTesting([&](webapps::InstallResultCode code) {
          result.install_code_before_fallback = code;
        }));
    command_manager().ScheduleCommand(std::move(command));
    result.callback_triggered = future.Wait();
    if (!result.callback_triggered) {
      return result;
    }
    result.installed_app_id = future.Get<webapps::AppId>();
    result.install_code = future.Get<webapps::InstallResultCode>();
    return result;
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  WebAppCommandManager& command_manager() {
    return provider()->command_manager();
  }

  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider()->web_contents_manager());
  }

  blink::mojom::ManifestPtr CreateManifest(GURL start_url,
                                           webapps::ManifestId manifest_id,
                                           bool icons) {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->name = kManifestName;
    manifest->start_url = start_url;
    manifest->id = manifest_id;
    if (icons) {
      blink::Manifest::ImageResource primary_icon;
      primary_icon.type = u"image/png";
      primary_icon.sizes.emplace_back(kIconSize, kIconSize);
      primary_icon.purpose.push_back(
          blink::mojom::ManifestImageResource_Purpose::ANY);
      primary_icon.src = GURL(kManifestIconUrl);
      manifest->icons.push_back(primary_icon);
    }
    return manifest;
  }

  std::u16string GetAppName(const webapps::AppId& app_id) {
    return base::UTF8ToUTF16(registrar().GetAppShortName(app_id));
  }
};

TEST_F(InstallFromSyncTest, SuccessWithManifest) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page with manifest.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  fake_page_state.opt_metadata =
      FakeWebContentsManager::CreateMetadataWithIconAndTitle(
          kDocumentTitle, kDocumentIconUrl, kIconSize);
  fake_page_state.manifest_before_default_processing =
      CreateManifest(kWebAppStartUrl, kWebAppManifestId, /*icons=*/true);

  // Icon state.
  web_contents_manager().GetOrCreateIconState(kManifestIconUrl).bitmaps = {
      gfx::test::CreateBitmap(kIconSize, kManifestIconColor)};

  InstallResult result =
      InstallFromSyncAndWait(kWebAppStartUrl, kWebAppManifestId);
  ASSERT_TRUE(result.callback_triggered);

  EXPECT_FALSE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the manifest info was installed.
  EXPECT_THAT(GetAppName(app_id), Eq(kManifestName));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kManifestIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kManifestIconColor));
}

TEST_F(InstallFromSyncTest, SuccessWithoutManifest) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page without manifest.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  fake_page_state.opt_metadata =
      FakeWebContentsManager::CreateMetadataWithIconAndTitle(
          kDocumentTitle, kDocumentIconUrl, kIconSize);

  // Icon state.
  web_contents_manager().GetOrCreateIconState(kDocumentIconUrl).bitmaps = {
      gfx::test::CreateBitmap(kIconSize, kDocumentIconColor)};

  InstallResult result =
      InstallFromSyncAndWait(kWebAppStartUrl, kWebAppManifestId);
  ASSERT_TRUE(result.callback_triggered);

  EXPECT_FALSE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the document & fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kDocumentIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kDocumentIconColor));
}

TEST_F(InstallFromSyncTest, SuccessManifestNoIcons) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page with manifest, no icons.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  fake_page_state.opt_metadata =
      FakeWebContentsManager::CreateMetadataWithIconAndTitle(
          kDocumentTitle, kDocumentIconUrl, kIconSize);
  fake_page_state.manifest_before_default_processing =
      CreateManifest(kWebAppStartUrl, kWebAppManifestId, /*icons=*/false);

  // Document icon state.
  web_contents_manager().GetOrCreateIconState(kDocumentIconUrl).bitmaps = {
      gfx::test::CreateBitmap(kIconSize, kDocumentIconColor)};

  InstallResult result =
      InstallFromSyncAndWait(kWebAppStartUrl, kWebAppManifestId);
  ASSERT_TRUE(result.callback_triggered);

  EXPECT_FALSE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the manifest was used & document icons were used.
  EXPECT_THAT(GetAppName(app_id), Eq(kManifestName));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kDocumentIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kDocumentIconColor));
}

TEST_F(InstallFromSyncTest, UrlRedirectUseFallback) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page redirects.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result =
      webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded;

  // Fallback icon state.
  web_contents_manager().GetOrCreateIconState(kFallbackIconUrl).bitmaps = {
      gfx::test::CreateBitmap(kIconSize, kFallbackIconColor)};

  InstallResult result =
      InstallFromSyncAndWait(kWebAppStartUrl, kWebAppManifestId);
  ASSERT_TRUE(result.callback_triggered);

  // Error occurred.
  ASSERT_TRUE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kInstallURLRedirected,
            result.install_code_before_fallback.value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kFallbackIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kFallbackIconColor));
}

TEST_F(InstallFromSyncTest, FallbackWebAppInstallInfo) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page redirects.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  fake_page_state.return_null_info = true;

  // Fallback icon state.
  web_contents_manager().GetOrCreateIconState(kFallbackIconUrl).bitmaps = {
      gfx::test::CreateBitmap(kIconSize, kFallbackIconColor)};

  InstallResult result =
      InstallFromSyncAndWait(kWebAppStartUrl, kWebAppManifestId);
  ASSERT_TRUE(result.callback_triggered);

  // Error occurred.
  ASSERT_TRUE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kGetWebAppInstallInfoFailed,
            result.install_code_before_fallback.value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kFallbackIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kFallbackIconColor));
}

TEST_F(InstallFromSyncTest, FallbackManifestIdMismatch) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page with manifest.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  fake_page_state.opt_metadata =
      FakeWebContentsManager::CreateMetadataWithIconAndTitle(
          kDocumentTitle, kDocumentIconUrl, kIconSize);
  fake_page_state.manifest_before_default_processing =
      CreateManifest(kWebAppStartUrl, kWebAppManifestId, /*icons=*/true);
  fake_page_state.manifest_before_default_processing->id =
      kOtherWebAppManifestId;

  // Icon state.
  web_contents_manager().GetOrCreateIconState(kDocumentIconUrl).bitmaps = {
      gfx::test::CreateBitmap(kIconSize, kDocumentIconColor)};

  InstallResult result =
      InstallFromSyncAndWait(kWebAppStartUrl, kWebAppManifestId);
  ASSERT_TRUE(result.callback_triggered);

  // Error occurred.
  ASSERT_TRUE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kExpectedAppIdCheckFailed,
            result.install_code_before_fallback.value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kDocumentIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kDocumentIconColor));
}

TEST_F(InstallFromSyncTest, TwoInstalls) {
  const webapps::AppId app_id1 = GenerateAppIdFromManifestId(kWebAppManifestId);
  const webapps::AppId app_id2 =
      GenerateAppIdFromManifestId(kOtherWebAppManifestId);

  // No need to set up our FakeWebContentsManager state, as the sync
  // installation will succeed even if the network is down.

  base::RunLoop loop1;
  base::RunLoop loop2;
  enum class Event {
    kApp1Installed,
    kNotifyApp1Installed,
    kNotifyApp1InstalledWithHooks,
    kApp2Installed,
    kNotifyApp2Installed,
    kNotifyApp2InstalledWithHooks,
  };

  std::vector<Event> events;
  std::vector<webapps::InstallResultCode> codes;
  WebAppInstallManagerObserverAdapter observer(&provider()->install_manager());
  observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        if (app_id == app_id1) {
          events.push_back(Event::kNotifyApp1Installed);
        } else {
          DCHECK_EQ(app_id, app_id2);
          events.push_back(Event::kNotifyApp2Installed);
        }
      }));
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        if (app_id == app_id1) {
          events.push_back(Event::kNotifyApp1InstalledWithHooks);
        } else {
          DCHECK_EQ(app_id, app_id2);
          events.push_back(Event::kNotifyApp2InstalledWithHooks);
        }
      }));

  std::unique_ptr<InstallFromSyncCommand> command =
      std::make_unique<InstallFromSyncCommand>(
          profile(), CreateParams(app_id1, kWebAppManifestId, kWebAppStartUrl),
          base::BindLambdaForTesting(
              [&](const webapps::AppId& id, webapps::InstallResultCode code) {
                events.push_back(Event::kApp1Installed);
                codes.push_back(code);
                loop1.Quit();
              }));
  command_manager().ScheduleCommand(std::move(command));
  command = std::make_unique<InstallFromSyncCommand>(
      profile(),
      CreateParams(app_id2, kOtherWebAppManifestId, kOtherWebAppStartUrl),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& id, webapps::InstallResultCode code) {
            events.push_back(Event::kApp2Installed);
            codes.push_back(code);
            loop2.Quit();
          }));
  command_manager().ScheduleCommand(std::move(command));
  loop1.Run();
  EXPECT_TRUE(command_manager().web_contents_for_testing());
  loop2.Run();
  content::WebContents* web_contents =
      command_manager().web_contents_for_testing();
  EXPECT_TRUE(web_contents);
  // Wait for web contents to be destroyed.
  content::WebContentsDestroyedWatcher web_contents_obserser(web_contents);
  web_contents_obserser.Wait();
  EXPECT_FALSE(command_manager().web_contents_for_testing());
  EXPECT_TRUE(registrar().IsInstalled(app_id1));
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
  std::vector<Event> expected;
  if (AreAppsLocallyInstalledBySync()) {
    expected = {
        Event::kNotifyApp1Installed,
        Event::kApp1Installed,
        Event::kNotifyApp1InstalledWithHooks,
        Event::kNotifyApp2Installed,
        Event::kApp2Installed,
        Event::kNotifyApp2InstalledWithHooks,
    };
  } else {
    expected = {
        Event::kNotifyApp1Installed,
        Event::kApp1Installed,
        Event::kNotifyApp2Installed,
        Event::kApp2Installed,
    };
  }
  EXPECT_THAT(events, ElementsAreArray(expected));
  EXPECT_THAT(codes,
              ElementsAre(webapps::InstallResultCode::kSuccessNewInstall,
                          webapps::InstallResultCode::kSuccessNewInstall));
}

TEST_F(InstallFromSyncTest, Shutdown) {
  const webapps::AppId app_id = GenerateAppIdFromManifestId(kWebAppManifestId);

  // Page with manifest, but have the manifest fetch cause the system to shut
  // down.
  auto& fake_page_state =
      web_contents_manager().GetOrCreatePageState(kWebAppStartUrl);
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
  fake_page_state.opt_metadata =
      FakeWebContentsManager::CreateMetadataWithIconAndTitle(
          kDocumentTitle, kDocumentIconUrl, kIconSize);
  fake_page_state.manifest_before_default_processing =
      CreateManifest(kWebAppStartUrl, kWebAppManifestId, /*icons=*/true);
  fake_page_state.on_manifest_fetch =
      base::BindLambdaForTesting([&]() { command_manager().Shutdown(); });

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  std::unique_ptr<InstallFromSyncCommand> command =
      std::make_unique<InstallFromSyncCommand>(
          profile(), CreateParams(app_id, kWebAppManifestId, kWebAppStartUrl),
          future.GetCallback());
  command_manager().ScheduleCommand(std::move(command));
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
  EXPECT_FALSE(registrar().IsInstalled(app_id));
}

}  // namespace
}  // namespace web_app
