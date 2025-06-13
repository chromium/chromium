// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/rewrite_diy_icons_command.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {

namespace {

gfx::Image LoadTestIcon(const char* filename) {
  base::FilePath data_root =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT);
  base::FilePath image_path =
      data_root.Append(FILE_PATH_LITERAL("chrome/test/data/web_apps/"))
          .Append(filename);
  std::string png_data;
  base::ReadFileToString(image_path, &png_data);
  return gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(png_data));
}

}  // namespace

class RewriteDiyIconsCommandTest : public WebAppTest {
 public:
  RewriteDiyIconsCommandTest() = default;
  ~RewriteDiyIconsCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    auto* provider = FakeWebAppProvider::Get(profile());
    provider->UseRealOsIntegrationManager();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    EXPECT_TRUE(test::UninstallAllWebApps(profile()));
    WebAppTest::TearDown();
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }

  // Get the OsIntegrationTestOverrideImpl instance.
  scoped_refptr<OsIntegrationTestOverrideImpl> GetTestOverride() {
    return OsIntegrationTestOverrideImpl::Get();
  }

  // Explicitly sets the DIY app's masked state to `masked_on_mac`.
  webapps::AppId InstallDiyApp(bool set_masked_on_mac = false) {
    auto install_info = std::make_unique<WebAppInstallInfo>(
        GURL("https://example.com/manifest"), GURL("https://example.com/app"));
    install_info->title = u"Test DIY App";
    install_info->scope = GURL("https://example.com/");
    install_info->is_diy_app = true;

    SkBitmap bitmap;
    bitmap.allocN32Pixels(32, 32);
    bitmap.eraseColor(SK_ColorBLUE);
    gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, 1.0f));
    install_info->icon_bitmaps.any[icon_size::k32] = bitmap;

    auto web_app_id = test::InstallWebApp(profile(), std::move(install_info));

    // Always explicitly set the masked state to ensure we start from a known
    // state. For migration testing, we need to ensure it starts as false.
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(web_app_id);
    CHECK(web_app);

    // Explicitly set the masked state as requested.
    web_app->SetDiyAppIconsMaskedOnMac(set_masked_on_mac);

    return web_app_id;
  }

  webapps::AppId InstallNonDiyApp() {
    auto install_info = std::make_unique<WebAppInstallInfo>(
        GURL("https://example.com/manifest"), GURL("https://example.com/app"));
    install_info->title = u"Test Regular App";
    install_info->scope = GURL("https://example.com/");
    install_info->is_diy_app = false;

    SkBitmap bitmap;
    bitmap.allocN32Pixels(32, 32);
    bitmap.eraseColor(SK_ColorBLUE);
    gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, 1.0f));
    install_info->icon_bitmaps.any[icon_size::k32] = bitmap;

    return test::InstallWebApp(profile(), std::move(install_info));
  }

  // Check if app's icon is marked as masked.
  bool IsDiyAppIconsMarkedMaskedOnMac(const webapps::AppId& app_id) {
    return registrar().IsDiyAppIconsMarkedMaskedOnMac(app_id);
  }
};

// Test that the actual RewriteDiyIcons command can be scheduled and executed.
TEST_F(RewriteDiyIconsCommandTest, ScheduleAndExecuteCommand) {
  base::HistogramTester histogram_tester;

  auto disable_icon_masking = testing::SetDisableIconMaskingForTesting(true);

  webapps::AppId app_id = InstallDiyApp(/*set_masked_on_mac=*/false);

  auto reenable_icon_masking = testing::SetDisableIconMaskingForTesting(false);

  // Get initial state and verify it's correctly set to false.
  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_TRUE(app);
  EXPECT_TRUE(app->is_diy_app());
  EXPECT_FALSE(IsDiyAppIconsMarkedMaskedOnMac(app_id));

  // Record initial shortcut state.
  std::string app_name = registrar().GetAppShortName(app_id);

  // Get the installed icon.
  std::optional<SkBitmap> installed_icon = GetTestOverride()->GetShortcutIcon(
      profile(), std::make_optional(GetTestOverride()->chrome_apps_folder()),
      app_id, app_name);

  // Load and compare with the original unmasked icon.
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(256, 256);
  expected_bitmap.eraseColor(SK_ColorBLUE);
  EXPECT_THAT(installed_icon.value(), gfx::test::EqualsBitmap(expected_bitmap));

  base::test::TestFuture<RewriteIconResult> future;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    fake_provider().scheduler().RewriteDiyIcons(app_id, future.GetCallback(),
                                                FROM_HERE);

    EXPECT_TRUE(future.Wait());
  }

  // Verify the rewrite operation result was successful.
  EXPECT_EQ(future.Get(), RewriteIconResult::kUpdateSucceeded);

  // Verify icon flag was updated from false to true.
  EXPECT_TRUE(IsDiyAppIconsMarkedMaskedOnMac(app_id));

  // Verify shortcut existence state (newly created or existing).
  EXPECT_TRUE(
      GetTestOverride()->IsShortcutCreated(profile(), app_id, app_name));

  // Get the current icon.
  std::optional<SkBitmap> current_icon = GetTestOverride()->GetShortcutIcon(
      profile(), std::make_optional(GetTestOverride()->chrome_apps_folder()),
      app_id, app_name);
  EXPECT_TRUE(current_icon.has_value());

  // compare with the original icon.
  EXPECT_THAT(current_icon.value(),
              ::testing::Not(gfx::test::EqualsBitmap(expected_bitmap)));

  // Load and compare with the expected icon.
  gfx::Image expected_icon = LoadTestIcon("diy_app_updated_128x128_icon.png");

  EXPECT_THAT(current_icon.value(),
              gfx::test::EqualsBitmap(expected_icon.AsBitmap()));

  histogram_tester.ExpectUniqueSample(
      "WebApp.DIY.IconMigrationResult",
      static_cast<int>(RewriteIconResult::kUpdateSucceeded), 1);
}

// Test that the actual RewriteDiyIcons command correctly handles non-DIY apps.
TEST_F(RewriteDiyIconsCommandTest, ScheduleCommandForNonDiyApp) {
  auto disable_icon_masking = testing::SetDisableIconMaskingForTesting(true);

  webapps::AppId app_id = InstallNonDiyApp();

  auto reenable_icon_masking = testing::SetDisableIconMaskingForTesting(false);

  // Get initial state.
  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_TRUE(app);
  EXPECT_FALSE(app->is_diy_app());
  EXPECT_FALSE(IsDiyAppIconsMarkedMaskedOnMac(app_id));

  std::string app_name = registrar().GetAppShortName(app_id);

  // Get the installed icon.
  std::optional<SkBitmap> installed_icon = GetTestOverride()->GetShortcutIcon(
      profile(), std::make_optional(GetTestOverride()->chrome_apps_folder()),
      app_id, app_name);

  // Load and compare with the original unmasked icon.
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(256, 256);
  expected_bitmap.eraseColor(SK_ColorBLUE);
  EXPECT_THAT(installed_icon.value(), gfx::test::EqualsBitmap(expected_bitmap));

  base::test::TestFuture<RewriteIconResult> future;
  fake_provider().scheduler().RewriteDiyIcons(app_id, future.GetCallback(),
                                              FROM_HERE);

  std::optional<SkBitmap> current_icon = GetTestOverride()->GetShortcutIcon(
      profile(), std::make_optional(GetTestOverride()->chrome_apps_folder()),
      app_id, app_name);
  EXPECT_TRUE(current_icon.has_value());
  EXPECT_THAT(current_icon.value(), gfx::test::EqualsBitmap(expected_bitmap));

  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(future.Get(), RewriteIconResult::kUnexpectedAppStateChange);

  EXPECT_FALSE(IsDiyAppIconsMarkedMaskedOnMac(app_id));
}

// Test that the actual RewriteDiyIcons command correctly handles already masked
// apps.
TEST_F(RewriteDiyIconsCommandTest, ScheduleCommandForAlreadyMaskedApp) {
  auto disable_icon_masking = testing::SetDisableIconMaskingForTesting(true);

  webapps::AppId app_id = InstallDiyApp(/*set_masked_on_mac=*/true);

  auto reenable_icon_masking = testing::SetDisableIconMaskingForTesting(false);
  // Get initial state.
  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_TRUE(app);
  EXPECT_TRUE(app->is_diy_app());
  EXPECT_TRUE(IsDiyAppIconsMarkedMaskedOnMac(app_id));

  std::string app_name = registrar().GetAppShortName(app_id);

  // Get the installed icon.
  std::optional<SkBitmap> installed_icon = GetTestOverride()->GetShortcutIcon(
      profile(), std::make_optional(GetTestOverride()->chrome_apps_folder()),
      app_id, app_name);

  // Load and compare with the original unmasked icon.
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(256, 256);
  expected_bitmap.eraseColor(SK_ColorBLUE);
  EXPECT_THAT(installed_icon.value(), gfx::test::EqualsBitmap(expected_bitmap));

  base::test::TestFuture<RewriteIconResult> future;
  fake_provider().scheduler().RewriteDiyIcons(app_id, future.GetCallback(),
                                              FROM_HERE);

  std::optional<SkBitmap> current_icon = GetTestOverride()->GetShortcutIcon(
      profile(), std::make_optional(GetTestOverride()->chrome_apps_folder()),
      app_id, app_name);
  EXPECT_TRUE(current_icon.has_value());
  EXPECT_THAT(current_icon.value(), gfx::test::EqualsBitmap(expected_bitmap));

  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(future.Get(), RewriteIconResult::kUnexpectedAppStateChange);

  EXPECT_TRUE(IsDiyAppIconsMarkedMaskedOnMac(app_id));
}

}  // namespace web_app
