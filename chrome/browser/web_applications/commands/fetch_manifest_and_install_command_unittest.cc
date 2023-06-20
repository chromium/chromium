// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {
namespace {

class FetchManifestAndInstallCommandTest : public WebAppTest {
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
    provider->SetIconManager(std::move(icon_manager));
    provider->SetWebAppUiManager(std::move(ui_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());

    web_contents_manager().SetUrlLoaded(web_contents(), kWebAppUrl);

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
      mojom::UserDisplayMode user_display_mode =
          mojom::UserDisplayMode::kBrowser) {
    return base::BindOnce(
        [](bool accept, mojom::UserDisplayMode user_display_mode,
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
    manifest->id = GenerateManifestIdFromStartUrlOnly(kWebAppUrl);
    manifest->display = blink::mojom::DisplayMode::kStandalone;
    return manifest;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider()->web_contents_manager());
  }

  void SetupPageState(
      blink::mojom::ManifestPtr opt_manifest = blink::mojom::ManifestPtr()) {
    auto& page_state = web_contents_manager().GetOrCreatePageState(kWebAppUrl);

    page_state.page_install_info = std::make_unique<WebAppInstallInfo>(
        GenerateManifestIdFromStartUrlOnly(kWebAppUrl));
    page_state.page_install_info->start_url = kWebAppUrl;
    page_state.has_service_worker = true;
    page_state.opt_manifest =
        opt_manifest ? std::move(opt_manifest) : CreateValidManifest();
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
  }

  void SetupIconState(IconsMap icons,
                      bool trigger_primary_page_changed = false,
                      int http_status_codes = 200) {
    for (const auto& [url, icon] : icons) {
      auto& icon_state = web_contents_manager().GetOrCreateIconState(url);
      icon_state.http_status_code = http_status_codes;
      icon_state.bitmaps = icon;
      icon_state.trigger_primary_page_changed_if_fetched =
          trigger_primary_page_changed;
    }
  }

  webapps::InstallResultCode InstallAndWait(
      const AppId& app_id,
      webapps::WebappInstallSource install_surface,
      WebAppInstallDialogCallback dialog_callback,
      bool use_fallback = false) {
    base::test::TestFuture<const AppId&, webapps::InstallResultCode>
        install_future;
    provider()->scheduler().FetchManifestAndInstall(
        install_surface, web_contents()->GetWeakPtr(),
        /*bypass_service_worker_check=*/false, std::move(dialog_callback),
        install_future.GetCallback(), use_fallback);
    EXPECT_TRUE(install_future.Wait());
    return install_future.Get<webapps::InstallResultCode>();
  }

 private:
  base::HistogramTester histogram_tester_;
  scoped_refptr<TestFileUtils> file_utils_;
  raw_ptr<FakeWebAppUiManager, DanglingUntriaged> fake_ui_manager_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ArcAppTest arc_test_;
  std::unique_ptr<arc::FakeIntentHelperHost> fake_intent_helper_host_;
  std::unique_ptr<arc::FakeIntentHelperInstance> fake_intent_helper_instance_;
#endif
};

TEST_F(FetchManifestAndInstallCommandTest, SuccessWithManifest) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const auto profile_count =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  SetupPageState();
  EXPECT_EQ(
      InstallAndWait(
          kWebAppId, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          CreateDialogCallback(true, mojom::UserDisplayMode::kStandalone)),
      webapps::InstallResultCode::kSuccessNewInstall);
  auto& registrar = provider()->registrar_unsafe();
  EXPECT_TRUE(registrar.IsLocallyInstalled(kWebAppId));
  EXPECT_EQ(1, fake_ui_manager()->num_reparent_tab_calls());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sanity check to confirm the experimental web app profile feature logic is
  // not running.
  auto* web_app = registrar.GetAppById(kWebAppId);
  ASSERT_NE(web_app, nullptr);
  EXPECT_TRUE(web_app->chromeos_data().has_value() &&
              !web_app->chromeos_data()->app_profile_path.has_value());
  EXPECT_EQ(g_browser_process->profile_manager()->GetNumberOfProfiles(),
            profile_count);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

TEST_F(FetchManifestAndInstallCommandTest, SuccessWithFallbackInstall) {
  auto web_app_info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(kWebAppUrl));
  web_app_info->start_url = kWebAppUrl;
  web_app_info->title = u"test app";
  web_app_info->scope = kWebAppUrl;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;

  SetupPageState();
  auto& page_state = web_contents_manager().GetOrCreatePageState(kWebAppUrl);
  page_state.page_install_info = std::move(web_app_info);

  EXPECT_EQ(InstallAndWait(
                kWebAppId, webapps::WebappInstallSource::MENU_CREATE_SHORTCUT,
                CreateDialogCallback(true, mojom::UserDisplayMode::kStandalone),
                /*use_fallback=*/true),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider()->registrar_unsafe().IsLocallyInstalled(kWebAppId));
  EXPECT_EQ(1, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(FetchManifestAndInstallCommandTest,
       FallbackInstallWithFailToGetInstallInfo) {
  // Not invoking SetUpPageStateForTesting() ensures that there is no data
  // set to be fetched.
  EXPECT_EQ(InstallAndWait(
                kWebAppId, webapps::WebappInstallSource::MENU_CREATE_SHORTCUT,
                CreateDialogCallback(true, mojom::UserDisplayMode::kStandalone),
                /*use_fallback=*/true),
            webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
  EXPECT_FALSE(provider()->registrar_unsafe().IsLocallyInstalled(kWebAppId));
  EXPECT_EQ(0, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(FetchManifestAndInstallCommandTest, SuccessWithoutReparent) {
  SetupPageState();
  EXPECT_EQ(InstallAndWait(
                kWebAppId, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                CreateDialogCallback(true, mojom::UserDisplayMode::kBrowser)),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_EQ(0, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(FetchManifestAndInstallCommandTest, UserInstallDeclined) {
  SetupPageState();
  EXPECT_EQ(
      InstallAndWait(
          kWebAppId, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          CreateDialogCallback(false, mojom::UserDisplayMode::kStandalone)),
      webapps::InstallResultCode::kUserInstallDeclined);
  EXPECT_FALSE(provider()->registrar_unsafe().IsLocallyInstalled(kWebAppId));
  EXPECT_EQ(0, fake_ui_manager()->num_reparent_tab_calls());
}

TEST_F(FetchManifestAndInstallCommandTest, Shutdown) {
  SetupPageState();
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

  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, std::move(dialog_callback),
      base::BindLambdaForTesting(
          [&](const AppId& id, webapps::InstallResultCode code) {
            result_populated = true;
            result = code;
          }),
      /*use_fallback=*/false);

  dialog_runloop.Run();
  provider()->command_manager().Shutdown();

  ASSERT_TRUE(result_populated);
  EXPECT_EQ(result,
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

TEST_F(FetchManifestAndInstallCommandTest, WebContentsDestroyed) {
  SetupPageState();

  base::test::TestFuture<const AppId&, webapps::InstallResultCode>
      install_future;
  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      install_future.GetCallback(), /*use_fallback=*/false);

  DeleteContents();
  ASSERT_TRUE(install_future.Wait());

  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kWebContentsDestroyed);
}

TEST_F(FetchManifestAndInstallCommandTest,
       MainFrameNavigationDifferentOriginDestroysCommand) {
  SetupPageState();

  base::test::TestFuture<void> manifest_fetch_future;
  base::test::TestFuture<const AppId&, webapps::InstallResultCode>
      install_future;

  auto& page_state = web_contents_manager().GetOrCreatePageState(kWebAppUrl);
  page_state.on_manifest_fetch = manifest_fetch_future.GetCallback();

  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      install_future.GetCallback(),
      /*use_fallback=*/false);

  // Wait till we reach an async process, like manifest fetching.
  EXPECT_TRUE(manifest_fetch_future.Wait());
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://abc.com/"));
  EXPECT_TRUE(install_future.Wait());

  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
}

TEST_F(FetchManifestAndInstallCommandTest,
       MainFrameNavigationSameOriginAllowsSuccessfulCompletion) {
  // Mimic a first navigation to the app url.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(kWebAppUrl);
  const GURL kWebAppUrl2 = GURL("https://example.com/path/index1.html");

  SetupPageState();
  base::test::TestFuture<void> manifest_fetch_future;
  base::test::TestFuture<const AppId&, webapps::InstallResultCode>
      install_future;

  auto& page_state = web_contents_manager().GetOrCreatePageState(kWebAppUrl);
  page_state.on_manifest_fetch = manifest_fetch_future.GetCallback();

  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      install_future.GetCallback(), /*use_fallback=*/false);

  // Wait till we reach an async process, then trigger navigation to another url
  // with the same origin.
  EXPECT_TRUE(manifest_fetch_future.Wait());
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(kWebAppUrl2);
  EXPECT_TRUE(install_future.Wait());

  // Verify that installation is successful.
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
}

TEST_F(FetchManifestAndInstallCommandTest, WriteDataToDisk) {
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

  int num_icons = icons_map.size();

  // TestingProfile creates temp directory if TestingProfile::path_ is empty
  // (i.e. if TestingProfile::Builder::SetPath was not called by a test
  // fixture)
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils().DirectoryExists(manifest_resources_directory));

  SetupPageState(std::move(manifest));
  SetupIconState(icons_map);

  EXPECT_EQ(InstallAndWait(kWebAppId,
                           webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                           CreateDialogCallback(true)),
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
      "WebApp.Icon.HttpStatusCodeClassOnCreate", http_code_class_ok, num_icons);
  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnSync",
                                      0);

  histogram_tester().ExpectBucketCount("WebApp.Icon.DownloadedResultOnCreate",
                                       IconsDownloadedResult::kCompleted, 1);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate",
      net::HttpStatusCode::HTTP_OK, 1);
}

TEST_F(FetchManifestAndInstallCommandTest, GetIcons_PrimaryPageChanged) {
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils().DirectoryExists(manifest_resources_directory));

  auto manifest = CreateValidManifest();

  // Prepare all the data to be fetched or downloaded.
  IconsMap icons_map;
  const GURL url = GURL("https://example.com/path");

  GURL icon_url = url.Resolve("color.png");
  manifest->icons.push_back(
      CreateSquareImageResource(icon_url, icon_size::k64, {IconPurpose::ANY}));
  icons_map[icon_url] = {CreateSquareIcon(icon_size::k64, SK_ColorBLUE)};

  SetupPageState(std::move(manifest));
  SetupIconState(icons_map, /*trigger_primary_page_changed=*/true);

  EXPECT_EQ(InstallAndWait(kWebAppId,
                           webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                           CreateDialogCallback(true)),
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

TEST_F(FetchManifestAndInstallCommandTest, GetIcons_IconNotFound) {
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils().DirectoryExists(manifest_resources_directory));

  auto manifest = CreateValidManifest();
  IconsMap icons_map;
  const GURL url = GURL("https://example.com/path");

  GURL icon_url = url.Resolve("color.png");
  manifest->icons.push_back(
      CreateSquareImageResource(icon_url, icon_size::k64, {IconPurpose::ANY}));
  SetupPageState(std::move(manifest));
  // Setting up an empty icons map should trigger a 404 response.
  SetupIconState(icons_map);

  EXPECT_EQ(InstallAndWait(kWebAppId,
                           webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                           CreateDialogCallback(true)),
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

TEST_F(FetchManifestAndInstallCommandTest, WriteDataToDiskFailed) {
  auto manifest = CreateValidManifest();
  IconsMap icons_map;

  GURL icon_url("https://example.com/app.ico");
  manifest->icons.push_back(
      CreateSquareImageResource(icon_url, icon_size::k64, {IconPurpose::ANY}));
  icons_map[icon_url] = {CreateSquareIcon(icon_size::k32, SK_ColorRED)};
  SetupPageState(std::move(manifest));
  SetupIconState(icons_map);

  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);

  EXPECT_TRUE(file_utils().CreateDirectory(manifest_resources_directory));

  // Induce an error: Simulate "Disk Full" for writing icon files.
  file_utils().SetRemainingDiskSpaceSize(1024);

  EXPECT_EQ(InstallAndWait(kWebAppId,
                           webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                           CreateDialogCallback(true)),
            webapps::InstallResultCode::kWriteDataFailed);

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils().DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils().IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(kWebAppId);
  EXPECT_FALSE(file_utils().DirectoryExists(app_dir));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(FetchManifestAndInstallCommandTest, IntentToPlayStore) {
  arc_test().app_instance()->set_is_installable(true);

  auto manifest = CreateValidManifest();
  blink::Manifest::RelatedApplication related_app;
  related_app.platform = u"chromeos_play";
  related_app.id = u"com.app.id";
  manifest->related_applications.push_back(std::move(related_app));
  SetupPageState(std::move(manifest));

  EXPECT_EQ(InstallAndWait(kWebAppId,
                           webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
                           CreateDialogCallback(true)),
            webapps::InstallResultCode::kIntentToPlayStore);
}
#endif

}  // namespace
}  // namespace web_app
