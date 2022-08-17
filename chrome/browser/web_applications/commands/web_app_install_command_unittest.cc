// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_install_command.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#endif

namespace web_app {
namespace {

class WebAppInstallCommandTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const AppId kWebAppId =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");

  void SetUp() override {
    WebAppTest::SetUp();

    file_utils_ = base::MakeRefCounted<TestFileUtils>();
    auto icon_manager =
        std::make_unique<WebAppIconManager>(profile(), file_utils_);

    auto ui_manager = std::make_unique<FakeWebAppUiManager>();
    fake_ui_manager_ = ui_manager.get();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    provider->SetIconManager(std::move(icon_manager));
    provider->SetWebAppUiManager(std::move(ui_manager));
    provider->SetRunSubsystemStartupTasks(true);

    test::AwaitStartWebAppProviderAndSubsystems(profile());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc_test_.SetUp(profile());

    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();
    fake_intent_helper_host_ = std::make_unique<arc::FakeIntentHelperHost>(
        arc_bridge_service->intent_helper());
    fake_intent_helper_instance_ =
        std::make_unique<arc::FakeIntentHelperInstance>();
    arc_bridge_service->intent_helper()->SetInstance(
        fake_intent_helper_instance_.get());
    WaitForInstanceReady(arc_bridge_service->intent_helper());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc_test_.arc_service_manager()
        ->arc_bridge_service()
        ->intent_helper()
        ->CloseInstance(fake_intent_helper_instance_.get());
    fake_intent_helper_instance_.reset();
    fake_intent_helper_host_.reset();
    arc_test_.TearDown();
#endif
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  FakeWebAppUiManager* fake_ui_manager() { return fake_ui_manager_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  TestFileUtils& file_utils() { return *file_utils_; }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ArcAppTest& arc_test() { return arc_test_; }
#endif

  WebAppInstallDialogCallback CreateDialogCallback(
      bool accept = true,
      UserDisplayMode user_display_mode = UserDisplayMode::kBrowser) {
    return base::BindOnce(
        [](bool accept, UserDisplayMode user_display_mode,
           content::WebContents* initiator_web_contents,
           std::unique_ptr<WebAppInstallInfo> web_app_info,
           WebAppInstallationAcceptanceCallback acceptance_callback) {
          web_app_info->user_display_mode = user_display_mode;
          std::move(acceptance_callback).Run(accept, std::move(web_app_info));
        },
        accept, user_display_mode);
  }

  blink::mojom::ManifestPtr CreateValidManifest() {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->name = u"foo";
    manifest->short_name = u"bar";
    manifest->start_url = kWebAppUrl;
    manifest->display = blink::mojom::DisplayMode::kStandalone;
    return manifest;
  }

  std::unique_ptr<WebAppDataRetriever> SetupFakeDataRetriever(
      IconsMap icons_map,
      IconsDownloadedResult result,
      int http_status_codes) {
    auto data_retriever = std::make_unique<FakeDataRetriever>();

    data_retriever->SetIconsDownloadedResult(result);

    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = http_status_codes;
    }
    data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));

    // Moves `icons_map` last.
    data_retriever->SetIcons(std::move(icons_map));
    return data_retriever;
  }

  webapps::InstallResultCode InstallAndWait(
      const AppId& app_id,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      webapps::WebappInstallSource install_surface,
      WebAppInstallDialogCallback dialog_callback,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      WebAppInstallFlow flow) {
    webapps::InstallResultCode result;
    base::RunLoop run_loop;
    provider()->command_manager().ScheduleCommand(
        std::make_unique<WebAppInstallCommand>(
            app_id, install_surface, std::move(web_app_info),
            std::move(opt_manifest), manifest_url, flow,
            std::move(dialog_callback),
            base::BindLambdaForTesting(
                [&](const AppId& id, webapps::InstallResultCode code) {
                  result = code;
                  run_loop.Quit();
                }),
            profile(), &provider()->install_finalizer(),
            std::move(data_retriever), web_contents()->GetWeakPtr()));
    run_loop.Run();
    return result;
  }

 private:
  base::HistogramTester histogram_tester_;
  scoped_refptr<TestFileUtils> file_utils_;
  raw_ptr<FakeWebAppUiManager> fake_ui_manager_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ArcAppTest arc_test_;
  std::unique_ptr<arc::FakeIntentHelperHost> fake_intent_helper_host_;
  std::unique_ptr<arc::FakeIntentHelperInstance> fake_intent_helper_instance_;
#endif
};

TEST_F(WebAppInstallCommandTest, SuccessWithManifest) {
  EXPECT_EQ(InstallAndWait(
                kWebAppId, std::make_unique<FakeDataRetriever>(),
                webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                CreateDialogCallback(true, UserDisplayMode::kStandalone),
                std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
                kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider()->registrar().IsLocallyInstalled(kWebAppId));
  EXPECT_EQ(1, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(WebAppInstallCommandTest, SuccessWithoutReparent) {
  EXPECT_EQ(InstallAndWait(
                kWebAppId, std::make_unique<FakeDataRetriever>(),
                webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                CreateDialogCallback(true, UserDisplayMode::kBrowser),
                std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
                kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_EQ(0, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(WebAppInstallCommandTest, UserInstallDeclined) {
  EXPECT_EQ(InstallAndWait(
                kWebAppId, std::make_unique<FakeDataRetriever>(),
                webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                CreateDialogCallback(false, UserDisplayMode::kStandalone),
                std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
                kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
            webapps::InstallResultCode::kUserInstallDeclined);
  EXPECT_FALSE(provider()->registrar().IsLocallyInstalled(kWebAppId));
  EXPECT_EQ(0, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(WebAppInstallCommandTest, Shutdown) {
  webapps::InstallResultCode result;
  bool result_populated = false;

  base::RunLoop dialog_runloop;
  auto dialog_callback = base::BindLambdaForTesting(
      [&](content::WebContents* initiator_web_contents,
          std::unique_ptr<WebAppInstallInfo> web_app_info,
          WebAppInstallationAcceptanceCallback acceptance_callback) {
        std::move(acceptance_callback).Run(true, std::move(web_app_info));
        dialog_runloop.Quit();
      });

  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppInstallCommand>(
          kWebAppId, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
          kWebAppManifestUrl, WebAppInstallFlow::kInstallSite,
          std::move(dialog_callback),
          base::BindLambdaForTesting(
              [&](const AppId& id, webapps::InstallResultCode code) {
                result_populated = true;
                result = code;
              }),
          profile(), &provider()->install_finalizer(),
          std::make_unique<FakeDataRetriever>(), web_contents()->GetWeakPtr()));

  dialog_runloop.Run();
  provider()->command_manager().Shutdown();

  ASSERT_TRUE(result_populated);
  EXPECT_EQ(result,
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

TEST_F(WebAppInstallCommandTest, WebContentsDestroyed) {
  webapps::InstallResultCode result;
  bool result_populated = false;

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppInstallCommand>(
          kWebAppId, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
          kWebAppManifestUrl, WebAppInstallFlow::kInstallSite,
          CreateDialogCallback(),
          base::BindLambdaForTesting(
              [&](const AppId& id, webapps::InstallResultCode code) {
                result_populated = true;
                result = code;
                loop.Quit();
              }),
          profile(), &provider()->install_finalizer(),
          std::make_unique<FakeDataRetriever>(), web_contents()->GetWeakPtr()));

  DeleteContents();
  loop.Run();

  ASSERT_TRUE(result_populated);
  EXPECT_EQ(result, webapps::InstallResultCode::kWebContentsDestroyed);
}

TEST_F(WebAppInstallCommandTest, WriteDataToDisk) {
  struct TestIconInfo {
    IconPurpose purpose;
    std::string icon_url_name;
    SkColor color;
    std::vector<SquareSizePx> sizes_px;
    std::string dir;
  };

  const std::array<TestIconInfo, 3> purpose_infos = {
      TestIconInfo{IconPurpose::ANY,
                   "any",
                   SK_ColorGREEN,
                   {icon_size::k16, icon_size::k512},
                   "Icons"},
      TestIconInfo{IconPurpose::MONOCHROME,
                   "monochrome",
                   SkColorSetARGB(0x80, 0x00, 0x00, 0x00),
                   {icon_size::k32, icon_size::k256},
                   "Icons Monochrome"},
      TestIconInfo{IconPurpose::MASKABLE,
                   "maskable",
                   SK_ColorRED,
                   {icon_size::k64, icon_size::k96, icon_size::k128},
                   "Icons Maskable"}};

  static_assert(
      purpose_infos.size() == static_cast<int>(IconPurpose::kMaxValue) -
                                  static_cast<int>(IconPurpose::kMinValue) + 1,
      "All purposes covered");

  auto manifest = CreateValidManifest();

  // Prepare all the data to be fetched or downloaded.
  IconsMap icons_map;
  const GURL url = GURL("https://example.com/path");

  for (const TestIconInfo& purpose_info : purpose_infos) {
    for (SquareSizePx s : purpose_info.sizes_px) {
      std::string size_str = base::NumberToString(s);
      GURL icon_url =
          url.Resolve(purpose_info.icon_url_name + size_str + ".png");

      manifest->icons.push_back(
          CreateSquareImageResource(icon_url, s, {purpose_info.purpose}));

      icons_map[icon_url] = {CreateSquareIcon(s, purpose_info.color)};
    }
  }
  int num_of_icons = icons_map.size();

  // TestingProfile creates temp directory if TestingProfile::path_ is empty
  // (i.e. if TestingProfile::Builder::SetPath was not called by a test fixture)
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils().DirectoryExists(manifest_resources_directory));

  EXPECT_EQ(
      InstallAndWait(kWebAppId,
                     SetupFakeDataRetriever(std::move(icons_map),
                                            IconsDownloadedResult::kCompleted,
                                            net::HttpStatusCode::HTTP_OK),
                     webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                     CreateDialogCallback(true),
                     std::make_unique<WebAppInstallInfo>(), std::move(manifest),
                     kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
      webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(file_utils().DirectoryExists(manifest_resources_directory));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils().DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils().IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(kWebAppId);
  EXPECT_TRUE(file_utils().DirectoryExists(app_dir));

  for (const TestIconInfo& purpose_info : purpose_infos) {
    SCOPED_TRACE(purpose_info.purpose);

    const base::FilePath icons_dir = app_dir.AppendASCII(purpose_info.dir);
    EXPECT_TRUE(file_utils().DirectoryExists(icons_dir));

    std::map<SquareSizePx, SkBitmap> pngs =
        ReadPngsFromDirectory(&file_utils(), icons_dir);

    // The install does ResizeIconsAndGenerateMissing() only for ANY icons.
    if (purpose_info.purpose == IconPurpose::ANY) {
      // Icons are generated for all mandatory sizes in GetIconSizes() in
      // addition to the input k16 and k512 sizes.
      EXPECT_EQ(GetIconSizes().size() + 2UL, pngs.size());
      // Excludes autogenerated sizes.
      for (SquareSizePx s : GetIconSizes()) {
        pngs.erase(s);
      }
    } else {
      EXPECT_EQ(purpose_info.sizes_px.size(), pngs.size());
    }

    for (SquareSizePx size_px : purpose_info.sizes_px) {
      SCOPED_TRACE(size_px);
      ASSERT_TRUE(base::Contains(pngs, size_px));

      SkBitmap icon_bitmap = pngs[size_px];
      EXPECT_EQ(icon_bitmap.width(), icon_bitmap.height());
      EXPECT_EQ(size_px, icon_bitmap.height());
      EXPECT_EQ(purpose_info.color, pngs[size_px].getColor(0, 0));
      pngs.erase(size_px);
    }

    EXPECT_TRUE(pngs.empty());
  }
  const int http_code_class_ok = 2;  // HTTP_OK is 200.
  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.HttpStatusCodeClassOnCreate", http_code_class_ok,
      num_of_icons);
  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnSync",
                                      0);

  histogram_tester().ExpectBucketCount("WebApp.Icon.DownloadedResultOnCreate",
                                       IconsDownloadedResult::kCompleted, 1);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate",
      net::HttpStatusCode::HTTP_OK, 1);
}

TEST_F(WebAppInstallCommandTest, GetIcons_PrimaryPageChanged) {
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils().DirectoryExists(manifest_resources_directory));

  IconsMap icons_map;
  EXPECT_EQ(
      InstallAndWait(
          kWebAppId,
          SetupFakeDataRetriever(std::move(icons_map),
                                 IconsDownloadedResult::kPrimaryPageChanged,
                                 net::HttpStatusCode::HTTP_OK),
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          CreateDialogCallback(true), std::make_unique<WebAppInstallInfo>(),
          CreateValidManifest(), kWebAppManifestUrl,
          WebAppInstallFlow::kInstallSite),
      webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(file_utils().DirectoryExists(manifest_resources_directory));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils().DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils().IsDirectoryEmpty(temp_dir));

  for (const std::string icon_dir :
       {"Icons", "Icons Monochrome", "Icons Maskable"}) {
    const base::FilePath app_dir =
        manifest_resources_directory.AppendASCII(kWebAppId);
    EXPECT_TRUE(file_utils().DirectoryExists(app_dir));

    const base::FilePath icons_dir = app_dir.AppendASCII(icon_dir);
    EXPECT_TRUE(file_utils().DirectoryExists(icons_dir));

    std::map<SquareSizePx, SkBitmap> pngs =
        ReadPngsFromDirectory(&file_utils(), icons_dir);
    if (icon_dir == "Icons") {
      // Auto generated ANY icons.
      EXPECT_EQ(GetIconSizes().size(), pngs.size());
      EXPECT_TRUE(ContainsOneIconOfEachSize(pngs));
    } else {
      EXPECT_TRUE(pngs.empty());
    }
  }

  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnCreate",
                                      0);
  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnSync",
                                      0);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedResultOnCreate",
      IconsDownloadedResult::kPrimaryPageChanged, 1);
  histogram_tester().ExpectTotalCount("WebApp.Icon.DownloadedResultOnSync", 0);

  histogram_tester().ExpectTotalCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate", 0);
  histogram_tester().ExpectTotalCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnSync", 0);
}

TEST_F(WebAppInstallCommandTest, GetIcons_IconNotFound) {
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils().DirectoryExists(manifest_resources_directory));

  IconsMap icons_map;
  AddEmptyIconToIconsMap(GURL("https://example.com/app.ico"), &icons_map);

  EXPECT_EQ(InstallAndWait(
                kWebAppId,
                SetupFakeDataRetriever(std::move(icons_map),
                                       IconsDownloadedResult::kCompleted,
                                       net::HttpStatusCode::HTTP_NOT_FOUND),
                webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                CreateDialogCallback(true),
                std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
                kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(file_utils().DirectoryExists(manifest_resources_directory));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils().DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils().IsDirectoryEmpty(temp_dir));

  for (const std::string icon_dir :
       {"Icons", "Icons Monochrome", "Icons Maskable"}) {
    const base::FilePath app_dir =
        manifest_resources_directory.AppendASCII(kWebAppId);
    EXPECT_TRUE(file_utils().DirectoryExists(app_dir));

    const base::FilePath icons_dir = app_dir.AppendASCII(icon_dir);
    EXPECT_TRUE(file_utils().DirectoryExists(icons_dir));

    std::map<SquareSizePx, SkBitmap> pngs =
        ReadPngsFromDirectory(&file_utils(), icons_dir);
    if (icon_dir == "Icons") {
      // Auto generated ANY icons.
      EXPECT_EQ(GetIconSizes().size(), pngs.size());
      EXPECT_TRUE(ContainsOneIconOfEachSize(pngs));
    } else {
      EXPECT_TRUE(pngs.empty());
    }
  }

  histogram_tester().ExpectBucketCount("WebApp.Icon.DownloadedResultOnCreate",
                                       IconsDownloadedResult::kCompleted, 1);
  histogram_tester().ExpectTotalCount("WebApp.Icon.DownloadedResultOnSync", 0);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate",
      net::HttpStatusCode::HTTP_NOT_FOUND, 1);
  histogram_tester().ExpectTotalCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnSync", 0);
}

TEST_F(WebAppInstallCommandTest, WriteDataToDiskFailed) {
  IconsMap icons_map;
  AddIconToIconsMap(GURL("https://example.com/app.ico"), icon_size::k512,
                    SK_ColorBLUE, &icons_map);

  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);

  EXPECT_TRUE(file_utils().CreateDirectory(manifest_resources_directory));

  // Induce an error: Simulate "Disk Full" for writing icon files.
  file_utils().SetRemainingDiskSpaceSize(1024);

  EXPECT_EQ(InstallAndWait(
                kWebAppId,
                SetupFakeDataRetriever(std::move(icons_map),
                                       IconsDownloadedResult::kCompleted,
                                       net::HttpStatusCode::HTTP_OK),
                webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                CreateDialogCallback(true),
                std::make_unique<WebAppInstallInfo>(), CreateValidManifest(),
                kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
            webapps::InstallResultCode::kWriteDataFailed);

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils().DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils().IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(kWebAppId);
  EXPECT_FALSE(file_utils().DirectoryExists(app_dir));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppInstallCommandTest, IntentToPlayStore) {
  arc_test().app_instance()->set_is_installable(true);

  auto manifest = CreateValidManifest();
  blink::Manifest::RelatedApplication related_app;
  related_app.platform = u"chromeos_play";
  related_app.id = u"com.app.id";
  manifest->related_applications.push_back(std::move(related_app));

  EXPECT_EQ(
      InstallAndWait(kWebAppId, std::make_unique<FakeDataRetriever>(),
                     webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                     CreateDialogCallback(true),
                     std::make_unique<WebAppInstallInfo>(), std::move(manifest),
                     kWebAppManifestUrl, WebAppInstallFlow::kInstallSite),
      webapps::InstallResultCode::kIntentToPlayStore);
}
#endif

}  // namespace
}  // namespace web_app
