// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h"

#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {
namespace {
constexpr int kIconSizeToUse = 96;
class ApplyPendingManifestUpdateCommandTest : public WebAppTest {
 public:
  ApplyPendingManifestUpdateCommandTest() = default;
  ApplyPendingManifestUpdateCommandTest(
      const ApplyPendingManifestUpdateCommandTest&) = delete;
  ApplyPendingManifestUpdateCommandTest& operator=(
      const ApplyPendingManifestUpdateCommandTest&) = delete;
  ~ApplyPendingManifestUpdateCommandTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        {features::kWebAppUsePrimaryIcon});
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->StartWithSubsystems();
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider);
  }

 protected:
  void SetupBasicInstallablePageState() {
    const GURL default_icon_url{"https://example.com/path/def_icon.png"};

    web_contents_manager().SetUrlLoaded(web_contents(), kAppUrl);
    auto& page_state = web_contents_manager().GetOrCreatePageState(kAppUrl);

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = default_icon_url;
    icon.sizes = {{kIconSizeToUse, kIconSizeToUse}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(default_icon_url).bitmaps = {
        gfx::test::CreateBitmap(kIconSizeToUse, SK_ColorCYAN)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kAppUrl);
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->has_valid_specified_start_url = true;

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  ManifestSilentUpdateCheckResult RunManifestSilentUpdateAndGetResult() {
    base::test::TestFuture<ManifestSilentUpdateCheckResult>
        manifest_silent_update_future;
    fake_provider().scheduler().ScheduleManifestSilentUpdate(
        *web_contents(), manifest_silent_update_future.GetCallback());

    EXPECT_TRUE(manifest_silent_update_future.Wait());
    return manifest_silent_update_future.Get();
  }

  ApplyPendingManifestUpdateResult RunManifestApplyPendingUpdateAndGetResult(
      const webapps::AppId& app_id) {
    base::test::TestFuture<ApplyPendingManifestUpdateResult>
        manifest_apply_pending_update_future;
    fake_provider().scheduler().ScheduleApplyPendingManifestUpdate(
        app_id, manifest_apply_pending_update_future.GetCallback());

    EXPECT_TRUE(manifest_apply_pending_update_future.Wait());
    return manifest_apply_pending_update_future.Get();
  }

  SkBitmap ChangePageIconAndGetBitmap() {
    auto& manifest = *web_contents_manager()
                          .GetOrCreatePageState(kAppUrl)
                          .manifest_before_default_processing;
    blink::Manifest::ImageResource new_icon;
    const GURL new_icon_url = GURL("https://example2.com/path/def_icon.png");
    new_icon.src = new_icon_url;
    new_icon.sizes = {{kIconSizeToUse, kIconSizeToUse}};
    new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    manifest.icons = {new_icon};

    // Set icon in content. Setting the icon color to YELLOW to trigger a more
    // than 10% image diff to create a pending update info when updating.
    SkBitmap updated_bitmap =
        gfx::test::CreateBitmap(kIconSizeToUse, SK_ColorYELLOW);
    web_contents_manager().GetOrCreateIconState(new_icon_url).bitmaps = {
        updated_bitmap};
    return updated_bitmap;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }

  bool AppHasPendingUpdateInfo(const webapps::AppId& app_id) {
    return provider()
        .registrar_unsafe()
        .GetAppById(app_id)
        ->pending_update_info()
        .has_value();
  }

  base::FilePath GetAppManifestIconsDir(Profile* profile,
                                        const webapps::AppId& app_id) {
    base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
    return GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
  }

  base::FilePath GetAppPendingTrustedIconsDir(Profile* profile,
                                              const webapps::AppId& app_id) {
    base::FilePath app_dir = GetAppManifestIconsDir(profile, app_id);
    return app_dir.AppendASCII("Pending Trusted Icons");
  }

  base::FilePath GetAppPendingManifestIconsDir(Profile* profile,
                                               const webapps::AppId& app_id) {
    base::FilePath app_dir = GetAppManifestIconsDir(profile, app_id);
    return app_dir.AppendASCII("Pending Manifest Icons");
  }

  base::FilePath GetAppTrustedIconsDir(Profile* profile,
                                       const webapps::AppId& app_id) {
    base::FilePath app_dir = GetAppManifestIconsDir(profile, app_id);
    return app_dir.AppendASCII("Trusted Icons");
  }

  SkBitmap LoadTestPNGAsBitmap(const base::FilePath& path) {
    std::string png_data;
    base::ReadFileToString(path, &png_data);
    return gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(png_data))
        .AsBitmap();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  const GURL kAppUrl = GURL("https://www.foo.bar/web_apps/basic.html");
};

}  // namespace

TEST_F(ApplyPendingManifestUpdateCommandTest,
       VerifyPendingIconsOverwriteIconsDirectory) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  SkBitmap updated_bitmap = ChangePageIconAndGetBitmap();

  EXPECT_EQ(RunManifestSilentUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  ASSERT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info());

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps>
      read_all_icons_future;
  provider().icon_manager().ReadAllIcons(app_id,
                                         read_all_icons_future.GetCallback());
  WebAppIconManager::WebAppBitmaps bitmaps = read_all_icons_future.Get();

  const SizeToBitmap& size_bitmaps_manifest =
      bitmaps.manifest_icons.GetBitmapsForPurpose(IconPurpose::ANY);
  auto icon_it = size_bitmaps_manifest.find(kIconSizeToUse);
  ASSERT_NE(icon_it, size_bitmaps_manifest.end());
  const SkBitmap& manifest_bitmap = icon_it->second;

  const SizeToBitmap& size_bitmaps_trusted =
      bitmaps.trusted_icons.GetBitmapsForPurpose(IconPurpose::ANY);
  auto trusted_icon_it = size_bitmaps_trusted.find(kIconSizeToUse);
  ASSERT_NE(trusted_icon_it, size_bitmaps_trusted.end());
  const SkBitmap& trusted_bitmap = trusted_icon_it->second;

  // Verify current manifest icon bitmaps are not the updated_bitmaps.
  EXPECT_THAT(manifest_bitmap,
              ::testing::Not(gfx::test::EqualsBitmap(updated_bitmap)));
  EXPECT_THAT(trusted_bitmap,
              ::testing::Not(gfx::test::EqualsBitmap(updated_bitmap)));

  EXPECT_EQ(RunManifestApplyPendingUpdateAndGetResult(app_id),
            ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully);

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps>
      pending_update_applied_read_all_icons_future;
  provider().icon_manager().ReadAllIcons(
      app_id, pending_update_applied_read_all_icons_future.GetCallback());

  WebAppIconManager::WebAppBitmaps pending_update_applied_bitmaps =
      pending_update_applied_read_all_icons_future.Get();

  const SizeToBitmap& size_bitmaps_updated_manifest =
      pending_update_applied_bitmaps.manifest_icons.GetBitmapsForPurpose(
          IconPurpose::ANY);
  auto updated_manifest_icon_it =
      size_bitmaps_updated_manifest.find(kIconSizeToUse);
  ASSERT_NE(updated_manifest_icon_it, size_bitmaps_updated_manifest.end());
  const SkBitmap& updated_manifest_bitmap = updated_manifest_icon_it->second;

  const SizeToBitmap& size_bitmaps_updated_trusted =
      pending_update_applied_bitmaps.trusted_icons.GetBitmapsForPurpose(
          IconPurpose::ANY);
  auto updated_trusted_icon_it =
      size_bitmaps_updated_trusted.find(kIconSizeToUse);
  ASSERT_NE(updated_trusted_icon_it, size_bitmaps_updated_trusted.end());
  const SkBitmap& updated_trusted_bitmap = updated_trusted_icon_it->second;

  EXPECT_THAT(updated_manifest_bitmap, gfx::test::EqualsBitmap(updated_bitmap));
  EXPECT_THAT(updated_trusted_bitmap, gfx::test::EqualsBitmap(updated_bitmap));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "WebApp.Update.ApplyPendingManifestUpdateResult"),
      BucketsAre(base::Bucket(
          ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully,
          /*count=*/1)));

  // TODO(crbug.com/445702843): Verify that the pending update info has been
  // deleted.
}

}  // namespace web_app
