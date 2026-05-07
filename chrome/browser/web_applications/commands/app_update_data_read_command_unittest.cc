// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/app_update_data_read_command.h"

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {

class AppUpdateDataReadCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  std::optional<WebAppIdentityUpdate> RunCommand(const webapps::AppId& app_id) {
    base::test::TestFuture<std::optional<WebAppIdentityUpdate>> test_future;
    provider()->scheduler().ReadAppUpdateDataFromDisk(
        app_id, test_future.GetCallback());
    return test_future.Get();
  }

  webapps::AppId InstallAppWithManifestIcon(const GURL& start_url,
                                            const SkBitmap& icon_bitmap) {
    auto install_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    install_info->title = u"Test App";
    OrderedSizeToBitmap icon_bitmaps;
    icon_bitmaps[kIconSizeForUpdateDialog] = icon_bitmap;
    install_info->icon_bitmaps.any = std::move(icon_bitmaps);
    webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(install_info));

    // Explicitly clear any trusted icon sizes metadata to ensure the
    // precondition of having no initial trusted icons is perfectly met.
    {
      ScopedRegistryUpdate update =
          provider()->sync_bridge_unsafe().BeginUpdate();
      WebApp* app = update->UpdateApp(app_id);
      if (app) {
        app->SetStoredTrustedIconSizes(IconPurpose::ANY, {});
        app->SetStoredTrustedIconSizes(IconPurpose::MASKABLE, {});
      }
    }
    return app_id;
  }

  // Set up a pending update info that "mimics" an app update to a
  // web app that has both trusted and manifest icons.
  // This behavior is hard to mimic on production, hence the usage
  // of ScopedRegistryUpdate to indirectly set up.
  testing::AssertionResult SetupPendingUpdateWithTrustedIcon(
      const webapps::AppId& app_id,
      const SkBitmap& pending_icon_bitmap,
      std::optional<std::string> new_name = std::nullopt) {
    {
      ScopedRegistryUpdate update =
          provider()->sync_bridge_unsafe().BeginUpdate();
      WebApp* app = update->UpdateApp(app_id);
      if (!app) {
        return testing::AssertionFailure() << "App " << app_id << " not found.";
      }

      proto::PendingUpdateInfo pending_info;
      if (new_name.has_value()) {
        pending_info.set_name(*new_name);
      }

      auto* trusted_icon = pending_info.add_trusted_icons();
      trusted_icon->set_url("https://example.com/trusted_icon.png");
      trusted_icon->set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
      trusted_icon->set_size_in_px(kIconSizeForUpdateDialog);

      auto* manifest_icon = pending_info.add_manifest_icons();
      manifest_icon->set_url("https://example.com/trusted_icon.png");
      manifest_icon->set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
      manifest_icon->set_size_in_px(kIconSizeForUpdateDialog);

      auto* downloaded_trusted = pending_info.add_downloaded_trusted_icons();
      downloaded_trusted->set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
      downloaded_trusted->add_icon_sizes(kIconSizeForUpdateDialog);

      auto* downloaded_manifest = pending_info.add_downloaded_manifest_icons();
      downloaded_manifest->set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
      downloaded_manifest->add_icon_sizes(kIconSizeForUpdateDialog);

      pending_info.set_was_ignored(false);
      app->SetPendingUpdateInfo(pending_info);
    }

    base::test::TestFuture<bool> write_future;
    IconBitmaps pending_trusted;
    pending_trusted.any[kIconSizeForUpdateDialog] = pending_icon_bitmap;

    IconBitmaps pending_manifest;
    pending_manifest.any[kIconSizeForUpdateDialog] = pending_icon_bitmap;

    provider()->icon_manager().WritePendingIconData(
        app_id, std::move(pending_trusted), std::move(pending_manifest),
        write_future.GetCallback());

    if (!write_future.Get()) {
      return testing::AssertionFailure()
             << "Failed to write pending icon data.";
    }
    return testing::AssertionSuccess();
  }
};

// Test Case 1: Visual difference is < 10%, applying a silent update.
TEST_F(AppUpdateDataReadCommandTest,
       SilentUpdateWhenImageDiffLessThanTenPercent) {
  base::HistogramTester histograms;
  SkBitmap old_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorRED);
  webapps::AppId app_id =
      InstallAppWithManifestIcon(GURL("https://example.com/"), old_bitmap);

  // Setup pending update with a slightly different trusted icon (<10% diff)
  SkBitmap new_bitmap;
  new_bitmap.allocN32Pixels(kIconSizeForUpdateDialog, kIconSizeForUpdateDialog);
  new_bitmap.eraseColor(SK_ColorRED);
  new_bitmap.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 0, 1, 1));
  ASSERT_TRUE(SetupPendingUpdateWithTrustedIcon(app_id, new_bitmap));

  std::optional<WebAppIdentityUpdate> result = RunCommand(app_id);

  // 1. The command finishes silently, so no identity update metadata is
  // returned to trigger a dialog.
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(
      histograms.GetAllSamples("WebApp.PendingUpdateData.ReadResult"),
      base::BucketsAre(base::Bucket(
          AppUpdateDataReadResult::kInsignificantChangeAfterUpdate, 1)));

  // Wait for the asynchronously scheduled ApplyPendingManifestUpdateCommand
  // to complete.
  provider()->command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* app = provider()->registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);

  // Verify a pending update is applied to the app correctly.
  EXPECT_FALSE(app->pending_update_info().has_value());
  EXPECT_FALSE(app->stored_trusted_icon_sizes(IconPurpose::ANY).empty());
  EXPECT_TRUE(app->stored_trusted_icon_sizes(IconPurpose::ANY)
                  .contains(kIconSizeForUpdateDialog));

  // The new trusted icon bitmap should be present in the persistent trusted
  // icon directories.
  EXPECT_TRUE(provider()->icon_manager().HasTrustedIcons(
      app_id, IconPurpose::ANY, {kIconSizeForUpdateDialog}));

  // Verify the bitmap on disk matches the new bitmap.
  base::test::TestFuture<IconMetadataFromDisk> read_future;
  provider()->icon_manager().ReadTrustedIconsWithFallbackToManifestIcons(
      app_id, {kIconSizeForUpdateDialog}, IconPurpose::ANY,
      read_future.GetCallback());
  IconMetadataFromDisk icon_metadata = read_future.Take();
  EXPECT_TRUE(icon_metadata.icons_map.contains(kIconSizeForUpdateDialog));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      icon_metadata.icons_map[kIconSizeForUpdateDialog], new_bitmap));
}

// Test Case 2: Visual difference is >= 10%, showing the update review dialog.
// This is evident from the command returning `kSuccess`.
TEST_F(AppUpdateDataReadCommandTest,
       ShowDialogWhenImageDiffGreaterThanTenPercent) {
  base::HistogramTester histograms;
  SkBitmap old_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorRED);
  webapps::AppId app_id =
      InstallAppWithManifestIcon(GURL("https://example.com/"), old_bitmap);

  // Setup pending update with a visually different trusted icon.
  SkBitmap new_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorBLUE);
  ASSERT_TRUE(SetupPendingUpdateWithTrustedIcon(app_id, new_bitmap));

  std::optional<WebAppIdentityUpdate> result = RunCommand(app_id);

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->icon_diff_is_insignificant);
  EXPECT_THAT(
      histograms.GetAllSamples("WebApp.PendingUpdateData.ReadResult"),
      base::BucketsAre(base::Bucket(AppUpdateDataReadResult::kSuccess, 1)));
}

// Test Case 3: Visual difference is < 10% but name changes, showing the update
// review dialog.
TEST_F(AppUpdateDataReadCommandTest,
       ShowDialogWhenNameChangesEvenIfIconDiffLessThanTenPercent) {
  base::HistogramTester histograms;
  SkBitmap old_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorRED);
  webapps::AppId app_id =
      InstallAppWithManifestIcon(GURL("https://example.com/"), old_bitmap);

  // Setup pending update with a slightly different trusted icon (<10% diff) AND
  // a new name. This is evident from the command returning `kSuccess`.
  SkBitmap new_bitmap;
  new_bitmap.allocN32Pixels(kIconSizeForUpdateDialog, kIconSizeForUpdateDialog);
  new_bitmap.eraseColor(SK_ColorRED);
  new_bitmap.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 0, 1, 1));
  ASSERT_TRUE(
      SetupPendingUpdateWithTrustedIcon(app_id, new_bitmap, "New Test App"));

  std::optional<WebAppIdentityUpdate> result = RunCommand(app_id);

  // Identity update metadata is returned to trigger the review dialog because
  // of the name change
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->icon_diff_is_insignificant);
  EXPECT_EQ(result->new_title, u"New Test App");

  // The histogram logs Success since the update has a significant change (name)
  // to show the user
  EXPECT_THAT(
      histograms.GetAllSamples("WebApp.PendingUpdateData.ReadResult"),
      base::BucketsAre(base::Bucket(AppUpdateDataReadResult::kSuccess, 1)));
}

}  // namespace web_app
