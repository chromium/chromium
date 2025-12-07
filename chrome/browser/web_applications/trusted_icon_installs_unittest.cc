// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {

namespace {

constexpr int kIconSize = 128;
constexpr int kTrustedIconSize = 256;
constexpr SkColor kManifestIconColor = SK_ColorRED;
constexpr SkColor kTrustedIconColor = SK_ColorBLUE;

using base::BucketsAre;
using testing::ElementsAre;

class TrustedIconInstallUnitTest : public WebAppTest {
 public:
  TrustedIconInstallUnitTest() = default;
  TrustedIconInstallUnitTest(const TrustedIconInstallUnitTest&) = delete;
  TrustedIconInstallUnitTest& operator=(const TrustedIconInstallUnitTest&) =
      delete;
  ~TrustedIconInstallUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
    web_contents_manager().SetUrlLoaded(web_contents(), app_url());
  }

 protected:
  void SetupBasicInstallablePageState(int allow_trusted_icons = true) {
    auto& page_state = web_contents_manager().GetOrCreatePageState(app_url());

    page_state.manifest_url = GURL("https://www.foo.bar/manifest.json");
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;

    // Set up icon states.
    blink::Manifest::ImageResource manifest_icon;
    manifest_icon.src = manifest_icon_url();
    manifest_icon.sizes = {{kIconSize, kIconSize}};
    manifest_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    web_contents_manager().GetOrCreateIconState(manifest_icon_url()).bitmaps = {
        gfx::test::CreateBitmap(kIconSize, kManifestIconColor)};

    blink::Manifest::ImageResource trusted_icon;
    if (allow_trusted_icons) {
      trusted_icon.src = trusted_icon_url();
      trusted_icon.sizes = {{kTrustedIconSize, kTrustedIconSize}};
      trusted_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
      web_contents_manager().GetOrCreateIconState(trusted_icon_url()).bitmaps =
          {gfx::test::CreateBitmap(kTrustedIconSize, kTrustedIconColor)};
    }

    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = app_url();
    manifest->id = GenerateManifestIdFromStartUrlOnly(app_url());
    manifest->scope = app_url().GetWithoutFilename();
    manifest->display = DisplayMode::kStandalone;
    manifest->name = u"Foo Bar App";
    manifest->icons = {manifest_icon};
    if (allow_trusted_icons) {
      manifest->icons.push_back(trusted_icon);
    }

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  webapps::AppId InstallExternallyManagedApp(
      ExternalInstallSource install_source) {
    ExternalInstallOptions install_options(
        app_url(), /*user_display_mode=*/std::nullopt, install_source);
    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    provider().scheduler().InstallExternallyManagedApp(
        install_options,
        /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());
    const ExternallyManagedAppManager::InstallResult& result =
        future.Get<ExternallyManagedAppManager::InstallResult>();
    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_TRUE(result.app_id.has_value());
    return *result.app_id;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  SizeToBitmap LoadIconsFromDisk(const webapps::AppId& app_id) {
    base::test::TestFuture<IconMetadataFromDisk> bitmap_future;
    provider().icon_manager().ReadTrustedIconsWithFallbackToManifestIcons(
        app_id, {32, 96, kTrustedIconSize}, IconPurpose::ANY,
        bitmap_future.GetCallback());
    EXPECT_TRUE(bitmap_future.Wait());
    IconMetadataFromDisk metadata = bitmap_future.Take();
    CHECK_EQ(metadata.purpose, IconPurpose::ANY);
    return metadata.icons_map;
  }

  void CorruptIconFilesOnDisk(const webapps::AppId& app_id) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    for (const SquareSizePx& size : web_app::SizesToGenerate()) {
      base::FilePath icon_path =
          provider().icon_manager().GetIconFilePathForTesting(
              app_id, IconPurpose::ANY, size);
      CHECK(!icon_path.empty());
      base::WriteFile(icon_path, "Not a PNG file");
    }
  }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }
  const GURL& app_url() { return app_url_; }
  const GURL& manifest_icon_url() { return manifest_icon_url_; }
  const GURL& trusted_icon_url() { return trusted_icon_url_; }

 private:
  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  base::test::ScopedFeatureList feature_list_{features::kWebAppUsePrimaryIcon};
  const GURL app_url_{"https://www.foo.bar/web_apps/basic.html"};
  const GURL manifest_icon_url_{
      "https://www.foo.bar/web_apps/manifest_icon.png"};
  const GURL trusted_icon_url_{"https://www.foo.bar/web_apps/trusted_icon.png"};
};

TEST_F(TrustedIconInstallUnitTest, UserInstall) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_NE(web_app, nullptr);

  // Verify manifest and trusted icon metadata.
  EXPECT_THAT(
      web_app->manifest_icons(),
      ElementsAre(apps::IconInfo(manifest_icon_url(), kIconSize),
                  apps::IconInfo(trusted_icon_url(), kTrustedIconSize)));
  EXPECT_THAT(
      web_app->trusted_icons(),
      ElementsAre(apps::IconInfo(trusted_icon_url(), kTrustedIconSize)));

  // Verify manifest and trusted icon disk info cached correctly.
  EXPECT_THAT(web_app->downloaded_icon_sizes(IconPurpose::ANY),
              ElementsAre(32, 48, 64, 96, 128, 256));
  EXPECT_THAT(web_app->stored_trusted_icon_sizes(IconPurpose::ANY),
              ElementsAre(32, 48, 64, 96, 128, 256));

  base::HistogramTester histogram_tester;
  // Verify that trusted icons are read correctly from disk.
  for (const auto& [size, bitmap] : LoadIconsFromDisk(app_id)) {
    EXPECT_EQ(kTrustedIconColor, bitmap.getColor(size / 2, size / 2));
  }

  // Icons are read properly from the trusted icons directory.
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.TrustedIcons.ReadResult"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(TrustedIconInstallUnitTest, PolicyInstall) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id =
      InstallExternallyManagedApp(ExternalInstallSource::kExternalPolicy);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_NE(web_app, nullptr);

  // Verify manifest and trusted icon metadata, they should be the same.
  EXPECT_THAT(
      web_app->manifest_icons(),
      ElementsAre(apps::IconInfo(manifest_icon_url(), kIconSize),
                  apps::IconInfo(trusted_icon_url(), kTrustedIconSize)));
  EXPECT_THAT(
      web_app->trusted_icons(),
      ElementsAre(apps::IconInfo(manifest_icon_url(), kIconSize),
                  apps::IconInfo(trusted_icon_url(), kTrustedIconSize)));

  // Verify manifest and trusted icon disk info cached correctly.
  EXPECT_THAT(web_app->downloaded_icon_sizes(IconPurpose::ANY),
              ElementsAre(32, 48, 64, 96, 128, 256));
  EXPECT_THAT(web_app->stored_trusted_icon_sizes(IconPurpose::ANY),
              ElementsAre(32, 48, 64, 96, 128, 256));

  base::HistogramTester histogram_tester;
  // Verify that trusted icons are read correctly from disk, including
  // fallbacks. The icons should be downloaded at the size they are provided, so
  // icon of size 256 should have the same color as determined in
  // `SetupBasicInstallablePageState()`.
  for (const auto& [size, bitmap] : LoadIconsFromDisk(app_id)) {
    if (size == kTrustedIconSize) {
      EXPECT_EQ(kTrustedIconColor, bitmap.getColor(size / 2, size / 2));
    } else {
      EXPECT_EQ(kManifestIconColor, bitmap.getColor(size / 2, size / 2));
    }
  }

  // Icons are read properly from the trusted icons directory.
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.TrustedIcons.ReadResult"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(TrustedIconInstallUnitTest, DefaultInstall) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id =
      InstallExternallyManagedApp(ExternalInstallSource::kExternalDefault);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_NE(web_app, nullptr);

  // Verify manifest and trusted icon metadata, they should be the same.
  EXPECT_THAT(
      web_app->manifest_icons(),
      ElementsAre(apps::IconInfo(manifest_icon_url(), kIconSize),
                  apps::IconInfo(trusted_icon_url(), kTrustedIconSize)));
  EXPECT_THAT(
      web_app->trusted_icons(),
      ElementsAre(apps::IconInfo(manifest_icon_url(), kIconSize),
                  apps::IconInfo(trusted_icon_url(), kTrustedIconSize)));

  // Verify manifest and trusted icon disk info cached correctly.
  EXPECT_THAT(web_app->downloaded_icon_sizes(IconPurpose::ANY),
              ElementsAre(32, 48, 64, 96, 128, 256));
  EXPECT_THAT(web_app->stored_trusted_icon_sizes(IconPurpose::ANY),
              ElementsAre(32, 48, 64, 96, 128, 256));

  base::HistogramTester histogram_tester;
  for (const auto& [size, bitmap] : LoadIconsFromDisk(app_id)) {
    if (size == kTrustedIconSize) {
      EXPECT_EQ(kTrustedIconColor, bitmap.getColor(size / 2, size / 2));
    } else {
      EXPECT_EQ(kManifestIconColor, bitmap.getColor(size / 2, size / 2));
    }
  }

  // Icons are read properly from the trusted icons directory.
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.TrustedIcons.ReadResult"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(TrustedIconInstallUnitTest, ManifestIconsFallbackOnIconCorruption) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_NE(web_app, nullptr);

  CorruptIconFilesOnDisk(app_id);
  base::HistogramTester histogram_tester;

  // Verify the fallback behavior of reading manifest icons when trusted icons
  // are corrupted.
  for (const auto& [size, bitmap] : LoadIconsFromDisk(app_id)) {
    if (size == kTrustedIconSize) {
      EXPECT_EQ(kTrustedIconColor, bitmap.getColor(size / 2, size / 2));
    } else {
      EXPECT_EQ(kManifestIconColor, bitmap.getColor(size / 2, size / 2));
    }
  }

  // Icons are read from the manifest icons directory on corruption of the
  // trusted icons.
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.TrustedIcons.ReadResult"),
              BucketsAre(base::Bucket(false, 1)));
}

}  // namespace

}  // namespace web_app
