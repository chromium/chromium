// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

class ExternallyManagedInstallCommandTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kWebAppScope = GURL("https://example.com/path/");
  const AppId kWebAppId =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");

  struct InstallResult {
    AppId installed_app_id;
    webapps::InstallResultCode install_code;
  };

  using BitmapData = std::map<SquareSizePx, SkBitmap>;

  InstallResult InstallAndWait(
      const ExternalInstallOptions& install_options,
      std::unique_ptr<WebAppDataRetriever> data_retriever) {
    base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool>
        future;
    provider()->command_manager().ScheduleCommand(
        std::make_unique<ExternallyManagedInstallCommand>(
            profile(), install_options, future.GetCallback(),
            web_contents()->GetWeakPtr(), std::move(data_retriever)));
    InstallResult result{.installed_app_id = future.Get<0>(),
                         .install_code = future.Get<1>()};
    return result;
  }

  blink::mojom::ManifestPtr CreateValidManifest() {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->name = u"Example App";
    manifest->short_name = u"App";
    manifest->start_url = kWebAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kWebAppUrl);
    manifest->display = blink::mojom::DisplayMode::kStandalone;
    return manifest;
  }

  void SetUp() override {
    WebAppTest::SetUp();
    auto shortcut_manager = std::make_unique<TestShortcutManager>(profile());
    shortcut_manager_ = shortcut_manager.get();
    FakeWebAppProvider::Get(profile())
        ->GetOsIntegrationManager()
        .AsTestOsIntegrationManager()
        ->SetShortcutManager(std::move(shortcut_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  apps::IconInfo GetMockIconInfo(const SquareSizePx& sq_size) {
    apps::IconInfo info;
    const GURL url("https://example.com/");
    info.url = url.Resolve("icon_any" + base::NumberToString(sq_size) + ".png");
    info.square_size_px = sq_size;
    return info;
  }

  IconsMap CreateAndLoadIconMapFromSizes(
      const std::vector<SquareSizePx>& sizes_px,
      const std::vector<SkColor>& colors,
      blink::mojom::Manifest* manifest) {
    DCHECK_EQ(colors.size(), sizes_px.size());
    IconsMap icons_map;
    const GURL url = GURL("https://example.com/");

    for (size_t i = 0; i < sizes_px.size(); i++) {
      GURL icon_url =
          url.Resolve("icon_any" + base::NumberToString(sizes_px[i]) + ".png");
      manifest->icons.push_back(
          CreateSquareImageResource(icon_url, sizes_px[i], {IconPurpose::ANY}));
      icons_map[icon_url] = {CreateSquareIcon(sizes_px[i], colors[i])};
    }
    return icons_map;
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  void LoadIconsFromDB(const AppId& app_id,
                       const std::vector<SquareSizePx>& sizes_px) {
    BitmapData icon_bitmaps;
    base::test::TestFuture<BitmapData> future;
    WebAppIconManager& icon_manager = provider()->icon_manager();

    // We can use this to test if icons of a specific size do not exist in the
    // DB. This is to ensure we do not trigger the same condition as a DCHECK
    // inside WebAppIconManager when calling ReadIcons().
    if (!icon_manager.HasIcons(app_id, IconPurpose::ANY, sizes_px)) {
      app_to_icons_data_[app_id] = icon_bitmaps;
      return;
    }

    icon_manager.ReadIcons(app_id, IconPurpose::ANY, sizes_px,
                           future.GetCallback());
    app_to_icons_data_[app_id] = future.Get();
  }

  std::vector<SquareSizePx> GetIconSizesForApp(const AppId& app_id) {
    DCHECK(base::Contains(app_to_icons_data_, app_id));
    std::vector<SquareSizePx> sizes;
    for (const auto& icon_data : app_to_icons_data_[app_id]) {
      sizes.push_back(icon_data.first);
    }
    return sizes;
  }

  std::vector<SkColor> GetIconColorsForApp(const AppId& app_id) {
    DCHECK(base::Contains(app_to_icons_data_, app_id));
    std::vector<SkColor> colors;
    for (const auto& icon_data : app_to_icons_data_[app_id]) {
      colors.push_back(icon_data.second.getColor(0, 0));
    }
    return colors;
  }

  FakeOsIntegrationManager* os_integration_manager() {
    return WebAppProvider::GetForTest(profile())
        ->os_integration_manager()
        .AsTestOsIntegrationManager();
  }
  TestShortcutManager* shortcut_manager() { return shortcut_manager_; }

 private:
  base::flat_map<AppId, BitmapData> app_to_icons_data_;
  raw_ptr<TestShortcutManager, DanglingUntriaged> shortcut_manager_;
};

TEST_F(ExternallyManagedInstallCommandTest, Success) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider()->registrar_unsafe().IsLocallyInstalled(
      result.installed_app_id));
}

TEST_F(ExternallyManagedInstallCommandTest, SuccessWithoutWebLoader) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider()->registrar_unsafe().IsLocallyInstalled(
      result.installed_app_id));
}

TEST_F(ExternallyManagedInstallCommandTest, GetWebAppInstallInfoFailed) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
  EXPECT_FALSE(provider()->registrar_unsafe().IsLocallyInstalled(
      result.installed_app_id));
}

TEST_F(ExternallyManagedInstallCommandTest,
       InstallWebAppWithParams_DisplayModeFromWebAppInstallInfo) {
  {
    GURL url("https://example1.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, /*user_display_mode=*/absl::nullopt,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
              provider()
                  ->registrar_unsafe()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
  {
    GURL url("https://example2.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, /*user_display_mode=*/absl::nullopt,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              provider()
                  ->registrar_unsafe()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
}

TEST_F(ExternallyManagedInstallCommandTest,
       InstallWebAppWithParams_DisplayModeOverrideByExternalInstallOptions) {
  {
    GURL url("https://example3.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, mojom::UserDisplayMode::kBrowser,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
              provider()
                  ->registrar_unsafe()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
  {
    GURL url("https://example4.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              provider()
                  ->registrar_unsafe()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
}

TEST_F(ExternallyManagedInstallCommandTest, UpgradeLock) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  base::flat_set<AppId> app_ids{
      GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl)};

  bool callback_command_run = false;
  auto callback_command = std::make_unique<CallbackCommand<AppLock>>(
      "", std::make_unique<AppLockDescription>(app_ids),
      base::BindLambdaForTesting(
          [&](AppLock&) { callback_command_run = true; }));

  bool callback_command_2_run = false;
  base::RunLoop callback_runloop;
  auto callback_command_2 = std::make_unique<CallbackCommand<AppLock>>(
      "", std::make_unique<AppLockDescription>(app_ids),
      base::BindLambdaForTesting([&](AppLock&) {
        callback_command_2_run = true;
        callback_runloop.Quit();
      }));

  base::RunLoop run_loop;
  InstallResult result;
  auto command = std::make_unique<ExternallyManagedInstallCommand>(
      profile(), install_options,
      base::BindLambdaForTesting([&](const AppId& app_id,
                                     webapps::InstallResultCode code,
                                     bool did_uninstall_and_replace) {
        result.install_code = code;
        result.installed_app_id = app_id;
        run_loop.Quit();
      }),
      web_contents()->GetWeakPtr(), std::move(data_retriever));

  // Schedules another callback command that acquires the same app lock after
  // current command upgrades to app lock.
  command->SetOnLockUpgradedCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        provider()->command_manager().ScheduleCommand(
            std::move(callback_command_2));
      }));

  provider()->command_manager().ScheduleCommand(std::move(command));
  // Immediately schedule a callback command, this will request the app lock
  // before the ExternallyManagedInstallCommand.
  provider()->command_manager().ScheduleCommand(std::move(callback_command));

  run_loop.Run();

  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider()->registrar_unsafe().IsLocallyInstalled(
      result.installed_app_id));

  EXPECT_TRUE(callback_command_run);

  EXPECT_FALSE(callback_command_2_run);

  callback_runloop.Run();
  EXPECT_TRUE(callback_command_2_run);
}

TEST_F(ExternallyManagedInstallCommandTest,
       IconDownloadSuccessOverwriteNoIconsBefore) {
  // First install the app normally.
  {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(result.install_code,
              webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_EQ(0u, provider()
                      ->registrar_unsafe()
                      .GetAppIconInfos(result.installed_app_id)
                      .size());
  }
  // Now install the same app with a manifest containing valid icons and
  // verify successful icon downloads and writes to DB.
  {
    std::vector<SquareSizePx> new_sizes{icon_size::k64, icon_size::k512};
    std::vector<SkColor> icon_colors{SK_ColorGREEN, SK_ColorRED};
    auto manifest = CreateValidManifest();
    IconsMap icons_map =
        CreateAndLoadIconMapFromSizes(new_sizes, icon_colors, manifest.get());
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    // Set up data retriever and load everything.
    auto new_data_retriever = std::make_unique<FakeDataRetriever>();
    new_data_retriever->SetIconsDownloadedResult(
        IconsDownloadedResult::kCompleted);
    new_data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));
    new_data_retriever->SetIcons(std::move(icons_map));
    new_data_retriever->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    new_data_retriever->SetEmptyRendererWebAppInstallInfo();

    ExternalInstallOptions new_install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    auto result =
        InstallAndWait(new_install_options, std::move(new_data_retriever));
    const AppId& installed_app_id = result.installed_app_id;

    EXPECT_EQ(result.install_code,
              webapps::InstallResultCode::kSuccessNewInstall);

    // Verify icon information.
    const std::vector<apps::IconInfo> icon_info =
        provider()->registrar_unsafe().GetAppIconInfos(installed_app_id);
    EXPECT_EQ(new_sizes.size(), icon_info.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), icon_info[0]);
    EXPECT_EQ(GetMockIconInfo(icon_size::k512), icon_info[1]);
    LoadIconsFromDB(installed_app_id, new_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), new_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), icon_colors);
  }
}

TEST_F(ExternallyManagedInstallCommandTest,
       IconDownloadSuccessOverwriteOldIcons) {
  // First install the app normally, having 1 icon only.
  {
    std::vector<SquareSizePx> old_sizes{icon_size::k256};
    std::vector<SkColor> old_colors{SK_ColorBLUE};
    auto manifest = CreateValidManifest();
    IconsMap icons_map =
        CreateAndLoadIconMapFromSizes(old_sizes, old_colors, manifest.get());
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);
    data_retriever->SetIconsDownloadedResult(IconsDownloadedResult::kCompleted);
    data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));
    data_retriever->SetIcons(std::move(icons_map));
    data_retriever->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));
    const AppId& installed_app_id = result.installed_app_id;

    EXPECT_EQ(result.install_code,
              webapps::InstallResultCode::kSuccessNewInstall);
    const std::vector<apps::IconInfo>& icons_info_pre_reinstall =
        provider()->registrar_unsafe().GetAppIconInfos(installed_app_id);
    EXPECT_EQ(old_sizes.size(), icons_info_pre_reinstall.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k256), icons_info_pre_reinstall[0]);
    LoadIconsFromDB(installed_app_id, old_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), old_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), old_colors);
  }
  // Now install the same app with a manifest containing different icons from
  // before, and verify that the icons are updated in the DB.
  {
    std::vector<SquareSizePx> new_sizes{icon_size::k64, icon_size::k512};
    std::vector<SkColor> new_colors{SK_ColorYELLOW, SK_ColorRED};
    auto new_manifest = CreateValidManifest();
    IconsMap new_icons_map = CreateAndLoadIconMapFromSizes(
        new_sizes, new_colors, new_manifest.get());
    DownloadedIconsHttpResults new_http_results;
    for (const auto& url_and_bitmap : new_icons_map) {
      new_http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    // Set up data retriever and load everything.
    auto new_data_retriever = std::make_unique<FakeDataRetriever>();
    new_data_retriever->SetIconsDownloadedResult(
        IconsDownloadedResult::kCompleted);
    new_data_retriever->SetDownloadedIconsHttpResults(
        std::move(new_http_results));
    new_data_retriever->SetIcons(std::move(new_icons_map));
    new_data_retriever->SetManifest(
        std::move(new_manifest),
        webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    new_data_retriever->SetEmptyRendererWebAppInstallInfo();

    ExternalInstallOptions new_install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    auto updated_result =
        InstallAndWait(new_install_options, std::move(new_data_retriever));
    const AppId& updated_app_id = updated_result.installed_app_id;

    EXPECT_EQ(updated_result.install_code,
              webapps::InstallResultCode::kSuccessNewInstall);

    // Verify icon information.
    const std::vector<apps::IconInfo> new_icon_info =
        provider()->registrar_unsafe().GetAppIconInfos(updated_app_id);
    EXPECT_EQ(new_sizes.size(), new_icon_info.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), new_icon_info[0]);
    EXPECT_EQ(GetMockIconInfo(icon_size::k512), new_icon_info[1]);
    LoadIconsFromDB(updated_app_id, new_sizes);
    EXPECT_EQ(GetIconSizesForApp(updated_app_id), new_sizes);
    EXPECT_EQ(GetIconColorsForApp(updated_app_id), new_colors);
  }
}

TEST_F(ExternallyManagedInstallCommandTest,
       IconDownloadFailDoesNotOverwriteOldIcons) {
  // First install the app normally, having 1 icon only.
  std::vector<SquareSizePx> old_sizes{icon_size::k64};
  std::vector<SkColor> old_colors{SK_ColorRED};
  {
    auto manifest = CreateValidManifest();
    IconsMap icons_map =
        CreateAndLoadIconMapFromSizes(old_sizes, old_colors, manifest.get());
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);
    data_retriever->SetIconsDownloadedResult(IconsDownloadedResult::kCompleted);
    data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));
    data_retriever->SetIcons(std::move(icons_map));
    data_retriever->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));
    const AppId& installed_app_id = result.installed_app_id;

    EXPECT_EQ(result.install_code,
              webapps::InstallResultCode::kSuccessNewInstall);
    const std::vector<apps::IconInfo>& icons_info_pre_reinstall =
        provider()->registrar_unsafe().GetAppIconInfos(installed_app_id);
    EXPECT_EQ(1u, icons_info_pre_reinstall.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), icons_info_pre_reinstall[0]);
    LoadIconsFromDB(installed_app_id, old_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), old_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), old_colors);
  }
  // Now install the same app with a manifest containing multiple icons, and
  // fail the icon downloading. This should prevent icons from getting
  // overwritten in the DB.
  {
    std::vector<SquareSizePx> new_sizes{icon_size::k256, icon_size::k512};
    std::vector<SkColor> new_colors{SK_ColorGREEN, SK_ColorBLUE};
    auto new_manifest = CreateValidManifest();
    IconsMap new_icons_map = CreateAndLoadIconMapFromSizes(
        new_sizes, new_colors, new_manifest.get());
    DownloadedIconsHttpResults new_http_results;
    for (const auto& url_and_bitmap : new_icons_map) {
      new_http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    // Set up data retriever and load everything.
    auto new_data_retriever = std::make_unique<FakeDataRetriever>();
    new_data_retriever->SetIconsDownloadedResult(
        IconsDownloadedResult::kAbortedDueToFailure);
    new_data_retriever->SetDownloadedIconsHttpResults(
        std::move(new_http_results));
    new_data_retriever->SetIcons(std::move(new_icons_map));
    new_data_retriever->SetManifest(
        std::move(new_manifest),
        webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    new_data_retriever->SetEmptyRendererWebAppInstallInfo();

    ExternalInstallOptions new_install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    auto updated_result =
        InstallAndWait(new_install_options, std::move(new_data_retriever));
    const AppId& installed_app_id = updated_result.installed_app_id;

    EXPECT_EQ(updated_result.install_code,
              webapps::InstallResultCode::kSuccessNewInstall);

    // Verify icon information, that new data is not written into the DB
    // and the old data still persists.
    const std::vector<apps::IconInfo> new_icon_info =
        provider()->registrar_unsafe().GetAppIconInfos(
            updated_result.installed_app_id);
    EXPECT_NE(new_sizes.size(), new_icon_info.size());
    LoadIconsFromDB(installed_app_id, new_sizes);
    // Trying to load new icons returns blank data because the new sizes and
    // colors have not been overwritten.
    EXPECT_EQ(0u, GetIconSizesForApp(installed_app_id).size());
    EXPECT_EQ(0u, GetIconColorsForApp(installed_app_id).size());

    EXPECT_EQ(old_sizes.size(), new_icon_info.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), new_icon_info[0]);
    LoadIconsFromDB(installed_app_id, old_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), old_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), old_colors);
  }
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ExternallyManagedInstallCommandTest, SuccessWithUninstallAndReplace) {
  GURL old_app_url("http://old-app.com");
  const AppId old_app =
      test::InstallDummyWebApp(profile(), "old_app", old_app_url);
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->url = old_app_url;
  shortcut_manager()->SetShortcutInfoForApp(old_app, std::move(shortcut_info));

  ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = false;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_startup = true;
  shortcut_manager()->SetAppExistingShortcuts(old_app_url, shortcut_locations);

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);
  install_options.uninstall_and_replace = {old_app};

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider()->registrar_unsafe().IsLocallyInstalled(
      result.installed_app_id));

  EXPECT_TRUE(os_integration_manager()->did_add_to_desktop());
  auto options = os_integration_manager()->get_last_install_options();
  EXPECT_FALSE(options->add_to_desktop);
  EXPECT_TRUE(options->add_to_quick_launch_bar);
  EXPECT_TRUE(options->os_hooks[OsHookType::kRunOnOsLogin]);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(
            result.installed_app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
    EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
              proto::RunOnOsLoginMode::WINDOWED);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
}  // namespace web_app
