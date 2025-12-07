// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h"

#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {
namespace {

constexpr int kAppIconSize = 10;
constexpr int kUpdatedAppIconSize = 56;
constexpr SkColor kAppIconColor = SK_ColorCYAN;
constexpr SkColor kUpdatedAppIconColor = SK_ColorYELLOW;

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
    provider->UseRealOsIntegrationManager();
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
    icon.sizes = {{kAppIconSize, kAppIconSize}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(default_icon_url).bitmaps = {
        gfx::test::CreateBitmap(kAppIconSize, kAppIconColor)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kAppUrl);
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->has_valid_specified_start_url = true;

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(kAppUrl)
        .manifest_before_default_processing;
  }

  ManifestSilentUpdateCheckResult RunManifestSilentUpdateAndGetResult() {
    base::test::TestFuture<ManifestSilentUpdateCompletionInfo>
        manifest_silent_update_future;
    fake_provider().scheduler().ScheduleManifestSilentUpdate(
        *web_contents(), /*previous_time_for_silent_icon_update=*/std::nullopt,
        manifest_silent_update_future.GetCallback());

    EXPECT_TRUE(manifest_silent_update_future.Wait());
    return manifest_silent_update_future.Take().result;
  }

  ApplyPendingManifestUpdateResult RunManifestApplyPendingUpdateAndGetResult(
      const webapps::AppId& app_id) {
    base::test::TestFuture<ApplyPendingManifestUpdateResult>
        manifest_apply_pending_update_future;
    fake_provider().scheduler().ScheduleApplyPendingManifestUpdate(
        app_id, /*keep_alive=*/nullptr, /*profile_keep_alive=*/nullptr,
        manifest_apply_pending_update_future.GetCallback());

    EXPECT_TRUE(manifest_apply_pending_update_future.Wait());
    return manifest_apply_pending_update_future.Get();
  }

  SkBitmap ChangePageIconAndGetBitmap(blink::mojom::ManifestPtr& manifest) {
    blink::Manifest::ImageResource new_icon;
    const GURL new_icon_url = GURL("https://example2.com/path/def_icon.png");
    new_icon.src = new_icon_url;
    new_icon.sizes = {{kUpdatedAppIconSize, kUpdatedAppIconSize}};
    new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    manifest->icons = {new_icon};

    // Set icon in content. Setting the icon color to YELLOW to trigger a more
    // than 10% image diff to create a pending update info when updating.
    SkBitmap updated_bitmap =
        gfx::test::CreateBitmap(kUpdatedAppIconSize, kUpdatedAppIconColor);
    web_contents_manager().GetOrCreateIconState(new_icon_url).bitmaps = {
        updated_bitmap};
    return updated_bitmap;
  }

  using WebAppBitmaps = WebAppIconManager::WebAppBitmaps;
  WebAppBitmaps ReadIconBitmapsFromIconManager(const webapps::AppId& app_id) {
    base::test::TestFuture<WebAppIconManager::WebAppBitmaps>
        read_all_icons_future;
    provider().icon_manager().ReadAllIcons(app_id,
                                           read_all_icons_future.GetCallback());
    return read_all_icons_future.Get();
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

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  const GURL kAppUrl = GURL("https://www.foo.bar/web_apps/basic.html");
};

}  // namespace

TEST_F(ApplyPendingManifestUpdateCommandTest, VerifyNameUpdatedSuccessfully) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(app_id),
            base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";

  EXPECT_EQ(RunManifestSilentUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);
  ASSERT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info());

  EXPECT_EQ(RunManifestApplyPendingUpdateAndGetResult(app_id),
            ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully);

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(app_id),
            base::UTF16ToUTF8(u"New Name"));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "WebApp.Update.ApplyPendingManifestUpdateResult"),
              BucketsAre(base::Bucket(
                  ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully,
                  /*count=*/1)));

  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(app_id)
                   ->pending_update_info()
                   .has_value());
}

// TODO(crbug.com/450578111): Refactor test.
TEST_F(ApplyPendingManifestUpdateCommandTest,
       VerifyPendingIconsUpdatedSuccessfully) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);
  auto& new_manifest = GetPageManifest();
  SkBitmap updated_bitmap = ChangePageIconAndGetBitmap(new_manifest);

  EXPECT_EQ(RunManifestSilentUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);
  auto* const web_app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app->pending_update_info());

  WebAppBitmaps bitmaps = ReadIconBitmapsFromIconManager(app_id);

  // Verify manifest icons
  EXPECT_EQ(7u, bitmaps.manifest_icons.any.size());
  EXPECT_TRUE(bitmaps.manifest_icons.any.contains(10));
  for (const auto& [size, bitmap] : bitmaps.manifest_icons.any) {
    EXPECT_EQ(kAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  // Verify trusted icons
  EXPECT_EQ(7u, bitmaps.trusted_icons.any.size());
  EXPECT_TRUE(bitmaps.trusted_icons.any.contains(10));
  for (const auto& [size, bitmap] : bitmaps.trusted_icons.any) {
    EXPECT_EQ(kAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  EXPECT_EQ(RunManifestApplyPendingUpdateAndGetResult(app_id),
            ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  WebAppBitmaps bitmaps_updated = ReadIconBitmapsFromIconManager(app_id);

  // Verify manifest icons
  EXPECT_EQ(7u, bitmaps_updated.manifest_icons.any.size());
  EXPECT_FALSE(bitmaps_updated.manifest_icons.any.contains(10));
  EXPECT_TRUE(bitmaps_updated.manifest_icons.any.contains(56));
  for (const auto& [size, bitmap] : bitmaps_updated.manifest_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  // Verify trusted icons
  EXPECT_EQ(7u, bitmaps_updated.manifest_icons.any.size());
  EXPECT_FALSE(bitmaps_updated.trusted_icons.any.contains(10));
  EXPECT_TRUE(bitmaps_updated.trusted_icons.any.contains(56));
  for (const auto& [size, bitmap] : bitmaps_updated.trusted_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

// TODO(crbug.com/40261124): Enable once PList parsing code is added to
// OsIntegrationTestOverride for Mac shortcut checking.
#if !BUILDFLAG(IS_MAC)
  // Verify that OS integration happened. Comparing it with updated_bitmap can
  // be flaky since GetShortcutIcon may return a bitmap of different size
  // depending on the OS. Instead, verifying the center bitmap color.
  std::optional<SkBitmap> installed_icon =
      fake_os_integration().GetShortcutIcon(profile(), std::nullopt, app_id,
                                            web_app->untranslated_name());
  ASSERT_TRUE(installed_icon.has_value());
  EXPECT_EQ(installed_icon->getColor(installed_icon->width() / 2,
                                     installed_icon->height() / 2),
            kUpdatedAppIconColor);
#endif

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "WebApp.Update.ApplyPendingManifestUpdateResult"),
      BucketsAre(base::Bucket(
          ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully,
          /*count=*/1)));

  // Verify the pending icon directories have been removed.
  EXPECT_FALSE(file_utils().PathExists(
      provider().icon_manager().GetAppPendingTrustedIconDirForTesting(app_id)));
  EXPECT_FALSE(file_utils().PathExists(
      provider().icon_manager().GetAppPendingManifestIconDirForTesting(
          app_id)));
  EXPECT_FALSE(web_app->pending_update_info().has_value());
}

// TODO(crbug.com/450578111): Refactor test.
TEST_F(ApplyPendingManifestUpdateCommandTest,
       VerifyPendingNameAndIconsUpdatedSuccessfully) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);
  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(app_id),
            base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";
  SkBitmap updated_bitmap = ChangePageIconAndGetBitmap(new_manifest);

  EXPECT_EQ(RunManifestSilentUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);
  auto* const web_app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app->pending_update_info());

  WebAppBitmaps bitmaps = ReadIconBitmapsFromIconManager(app_id);

  // Verify manifest icons
  EXPECT_EQ(7u, bitmaps.manifest_icons.any.size());
  EXPECT_TRUE(bitmaps.manifest_icons.any.contains(10));
  for (const auto& [size, bitmap] : bitmaps.manifest_icons.any) {
    EXPECT_EQ(kAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  // Verify trusted icons
  EXPECT_EQ(7u, bitmaps.trusted_icons.any.size());
  EXPECT_TRUE(bitmaps.trusted_icons.any.contains(10));
  for (const auto& [size, bitmap] : bitmaps.trusted_icons.any) {
    EXPECT_EQ(kAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  EXPECT_EQ(
      RunManifestApplyPendingUpdateAndGetResult(app_id),
      ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully);

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(app_id),
            base::UTF16ToUTF8(u"New Name"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  WebAppBitmaps bitmaps_updated = ReadIconBitmapsFromIconManager(app_id);

  // Verify manifest icons
  EXPECT_EQ(7u, bitmaps_updated.manifest_icons.any.size());
  EXPECT_FALSE(bitmaps_updated.manifest_icons.any.contains(10));
  EXPECT_TRUE(bitmaps_updated.manifest_icons.any.contains(56));
  for (const auto& [size, bitmap] : bitmaps_updated.manifest_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  // Verify trusted icons
  EXPECT_EQ(7u, bitmaps_updated.manifest_icons.any.size());
  EXPECT_FALSE(bitmaps_updated.trusted_icons.any.contains(10));
  EXPECT_TRUE(bitmaps_updated.trusted_icons.any.contains(56));
  for (const auto& [size, bitmap] : bitmaps_updated.trusted_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

// TODO(crbug.com/40261124): Enable once PList parsing code is added to
// OsIntegrationTestOverride for Mac shortcut checking.
#if !BUILDFLAG(IS_MAC)
  // Verify that OS integration happened. Comparing it with updated_bitmap can
  // be flaky since GetShortcutIcon may return a bitmap of different size
  // depending on the OS. Instead, verifying the center bitmap color.
  std::optional<SkBitmap> installed_icon =
      fake_os_integration().GetShortcutIcon(profile(), std::nullopt, app_id,
                                            web_app->untranslated_name());
  ASSERT_TRUE(installed_icon.has_value());
  EXPECT_EQ(installed_icon->getColor(installed_icon->width() / 2,
                                     installed_icon->height() / 2),
            kUpdatedAppIconColor);
#endif

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "WebApp.Update.ApplyPendingManifestUpdateResult"),
      BucketsAre(base::Bucket(
          ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully,
          /*count=*/1)));

  // Verify the pending icon directories have been removed.
  EXPECT_FALSE(file_utils().PathExists(
      provider().icon_manager().GetAppPendingTrustedIconDirForTesting(app_id)));
  EXPECT_FALSE(file_utils().PathExists(
      provider().icon_manager().GetAppPendingManifestIconDirForTesting(
          app_id)));
  EXPECT_FALSE(web_app->pending_update_info().has_value());
}

TEST_F(ApplyPendingManifestUpdateCommandTest,
       VerifyPendingIconsDiffPurposeUpdatedSuccessfully) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  const GURL icon_url = GURL("https://example.com/path/def_icon.png");
  const GURL icon_any_url = GURL("https://example2.com/path/def_icon.png");

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            icon_url);
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  // Set up a maskable icon to use in the update.
  blink::Manifest::ImageResource updated_icon;
  int icon_size_maskable = 256;
  updated_icon.src = icon_url;
  updated_icon.sizes = {{icon_size_maskable, icon_size_maskable}};
  updated_icon.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};

  // Since MASKABLE is not supported windows and linux for trusted icons, one
  // ANY icon is required to be chosen as the trusted icon.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  blink::Manifest::ImageResource updated_any_icon;
  int icon_size_any = 128;
  updated_any_icon.src = icon_any_url;
  updated_any_icon.sizes = {{icon_size_any, icon_size_any}};
  updated_any_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
#endif

  auto& new_manifest = GetPageManifest();

  // Adding the ANY icon that will be chosen as the trusted icon on windows and
  // linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  new_manifest->icons = {updated_icon, updated_any_icon};
  // Make icon diff larger than 10% by changing the color to RED.
  const SkBitmap updated_any_bitmaps =
      gfx::test::CreateBitmap(icon_size_any, kUpdatedAppIconColor);
  web_contents_manager().GetOrCreateIconState(icon_any_url).bitmaps = {
      updated_any_bitmaps};
#else  // Mac and ChromeOS should get MASKABLE
  new_manifest->icons = {updated_icon};
#endif

  // Make icon diff larger than 10% by changing the color to RED.
  const SkBitmap updated_bitmap =
      gfx::test::CreateBitmap(icon_size_maskable, kUpdatedAppIconColor);
  web_contents_manager().GetOrCreateIconState(icon_url).bitmaps = {
      updated_bitmap};

  EXPECT_EQ(RunManifestSilentUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);
  auto* const web_app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app->pending_update_info());

  WebAppBitmaps bitmaps = ReadIconBitmapsFromIconManager(app_id);

  // Verify manifest icons
  EXPECT_EQ(7u, bitmaps.manifest_icons.any.size());
  EXPECT_TRUE(bitmaps.manifest_icons.maskable.empty());
  EXPECT_TRUE(bitmaps.manifest_icons.monochrome.empty());

  // Verify the color of the icon
  for (const auto& [size, bitmap] : bitmaps.manifest_icons.any) {
    EXPECT_EQ(kAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  // Verify trusted icons
  EXPECT_EQ(7u, bitmaps.trusted_icons.any.size());
  EXPECT_TRUE(bitmaps.trusted_icons.maskable.empty());
  EXPECT_TRUE(bitmaps.trusted_icons.monochrome.empty());

  // Verify the color of the icon
  for (const auto& [size, bitmap] : bitmaps.trusted_icons.any) {
    EXPECT_EQ(kAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  EXPECT_EQ(RunManifestApplyPendingUpdateAndGetResult(app_id),
            ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully);

  WebAppBitmaps bitmaps_updated = ReadIconBitmapsFromIconManager(app_id);

  // Verify manifest icons
  EXPECT_EQ(6u, bitmaps_updated.manifest_icons.any.size());
  EXPECT_EQ(1u, bitmaps_updated.manifest_icons.maskable.size());
  EXPECT_TRUE(bitmaps_updated.manifest_icons.monochrome.empty());

  // Verify the color of the maskable icon
  for (const auto& [size, bitmap] : bitmaps_updated.manifest_icons.maskable) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  // Verify the color of the manifest any icons
  for (const auto& [size, bitmap] : bitmaps_updated.manifest_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

  // Verify trusted icons
  EXPECT_EQ(6u, bitmaps_updated.trusted_icons.any.size());
  EXPECT_TRUE(bitmaps_updated.trusted_icons.maskable.empty());
  EXPECT_TRUE(bitmaps_updated.trusted_icons.monochrome.empty());

  // Verify the color of the icon
  for (const auto& [size, bitmap] : bitmaps_updated.trusted_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }
#else  // Mac and ChromeOS
  // Verify trusted icons
  EXPECT_TRUE(bitmaps_updated.trusted_icons.any.empty());
  EXPECT_EQ(6u, bitmaps_updated.trusted_icons.maskable.size());
  EXPECT_TRUE(bitmaps_updated.trusted_icons.monochrome.empty());

  // Verify the color of the maskable icon
  for (const auto& [size, bitmap] : bitmaps_updated.trusted_icons.any) {
    EXPECT_EQ(kUpdatedAppIconColor,
              bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
  }

#endif

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "WebApp.Update.ApplyPendingManifestUpdateResult"),
      BucketsAre(base::Bucket(
          ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully,
          /*count=*/1)));

  // Verify the pending icon directories have been removed.
  EXPECT_FALSE(file_utils().PathExists(
      provider().icon_manager().GetAppPendingTrustedIconDirForTesting(app_id)));
  EXPECT_FALSE(file_utils().PathExists(
      provider().icon_manager().GetAppPendingManifestIconDirForTesting(
          app_id)));
  EXPECT_FALSE(web_app->pending_update_info().has_value());
}

}  // namespace web_app
