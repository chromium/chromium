// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/app_migration_data_read_command.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/model/pending_migration_info.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {
namespace {

constexpr char kSourceAppUrl[] = "https://source.example.com/";
constexpr char kTargetAppUrl[] = "https://target.example.com/";

}  // namespace

class AppMigrationDataReadCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  UpdateMetadata RunCommand(const webapps::AppId& old_app_id,
                            const webapps::AppId& new_app_id,
                            bool is_forced_migration_on_startup) {
    base::test::TestFuture<UpdateMetadata> future;
    provider()->scheduler().ReadAppMigrationDataFromDisk(
        old_app_id, new_app_id, is_forced_migration_on_startup,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  webapps::AppId InstallAppWithIcon(const GURL& start_url,
                                    const std::u16string& title,
                                    const SkBitmap& icon_bitmap) {
    auto install_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    install_info->title = title;
    std::map<SquareSizePx, SkBitmap> icon_bitmaps;
    icon_bitmaps[kIconSizeForUpdateDialog] = icon_bitmap;
    install_info->icon_bitmaps.any = std::move(icon_bitmaps);
    return test::InstallWebApp(profile(), std::move(install_info));
  }

  void SetPendingMigrationInfo(const webapps::AppId& app_id,
                               const webapps::ManifestId& manifest_id) {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id);
    PendingMigrationInfo info(manifest_id, MigrationBehavior::kSuggest);
    app->SetPendingMigrationInfo(info);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppMigrationDataReadCommandTest, SignificantIconChange) {
  SkBitmap cyan_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorCYAN);
  webapps::AppId app_id_source =
      InstallAppWithIcon(GURL(kSourceAppUrl), u"Source App", cyan_bitmap);

  SkBitmap yellow_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorYELLOW);
  webapps::AppId app_id_target =
      InstallAppWithIcon(GURL(kTargetAppUrl), u"Target App", yellow_bitmap);

  SetPendingMigrationInfo(
      app_id_source, GenerateManifestIdFromStartUrlOnly(GURL(kTargetAppUrl)));

  UpdateMetadata result = RunCommand(app_id_source, app_id_target,
                                     /*is_forced_migration_on_startup=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->old_title, u"Source App");
  EXPECT_EQ(result->new_title, u"Target App");
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->old_icon.AsBitmap(), cyan_bitmap));
  ASSERT_TRUE(result->new_icon.has_value());
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->new_icon->AsBitmap(), yellow_bitmap));
  EXPECT_EQ(result->old_start_url, GURL(kSourceAppUrl));
  EXPECT_EQ(result->new_start_url, GURL(kTargetAppUrl));
  EXPECT_FALSE(result->is_forced_migration);
  EXPECT_FALSE(result->icon_diff_is_insignificant);
}

TEST_F(AppMigrationDataReadCommandTest, InsignificantIconChange) {
  SkBitmap cyan_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorCYAN);
  webapps::AppId app_id_source =
      InstallAppWithIcon(GURL(kSourceAppUrl), u"Source App", cyan_bitmap);

  SkBitmap changed_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorCYAN);
  // Change 9 rows (less than 10% of 96 rows)
  changed_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorRED);
  webapps::AppId app_id_target =
      InstallAppWithIcon(GURL(kTargetAppUrl), u"Target App", changed_bitmap);

  SetPendingMigrationInfo(
      app_id_source, GenerateManifestIdFromStartUrlOnly(GURL(kTargetAppUrl)));

  UpdateMetadata result = RunCommand(app_id_source, app_id_target,
                                     /*is_forced_migration_on_startup=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->old_title, u"Source App");
  EXPECT_EQ(result->new_title, u"Target App");
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->old_icon.AsBitmap(), cyan_bitmap));
  ASSERT_TRUE(result->new_icon.has_value());
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->new_icon->AsBitmap(), changed_bitmap));
  EXPECT_EQ(result->old_start_url, GURL(kSourceAppUrl));
  EXPECT_EQ(result->new_start_url, GURL(kTargetAppUrl));
  EXPECT_FALSE(result->is_forced_migration);
  EXPECT_TRUE(result->icon_diff_is_insignificant);
}

TEST_F(AppMigrationDataReadCommandTest, IdenticalIcon) {
  SkBitmap cyan_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorCYAN);
  webapps::AppId app_id_source =
      InstallAppWithIcon(GURL(kSourceAppUrl), u"Source App", cyan_bitmap);
  webapps::AppId app_id_target =
      InstallAppWithIcon(GURL(kTargetAppUrl), u"Target App", cyan_bitmap);

  SetPendingMigrationInfo(
      app_id_source, GenerateManifestIdFromStartUrlOnly(GURL(kTargetAppUrl)));

  UpdateMetadata result = RunCommand(app_id_source, app_id_target,
                                     /*is_forced_migration_on_startup=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->old_title, u"Source App");
  EXPECT_EQ(result->new_title, u"Target App");
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->old_icon.AsBitmap(), cyan_bitmap));
  ASSERT_TRUE(result->new_icon.has_value());
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->new_icon->AsBitmap(), cyan_bitmap));
  EXPECT_EQ(result->old_start_url, GURL(kSourceAppUrl));
  EXPECT_EQ(result->new_start_url, GURL(kTargetAppUrl));
  EXPECT_FALSE(result->is_forced_migration);
  EXPECT_TRUE(result->icon_diff_is_insignificant);
}

TEST_F(AppMigrationDataReadCommandTest, ForcedMigrationIgnoresNewIcon) {
  SkBitmap cyan_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorCYAN);
  webapps::AppId app_id_source =
      InstallAppWithIcon(GURL(kSourceAppUrl), u"Source App", cyan_bitmap);

  SkBitmap yellow_bitmap =
      gfx::test::CreateBitmap(kIconSizeForUpdateDialog, SK_ColorYELLOW);
  webapps::AppId app_id_target =
      InstallAppWithIcon(GURL(kTargetAppUrl), u"Target App", yellow_bitmap);

  SetPendingMigrationInfo(
      app_id_source, GenerateManifestIdFromStartUrlOnly(GURL(kTargetAppUrl)));

  UpdateMetadata result = RunCommand(app_id_source, app_id_target,
                                     /*is_forced_migration_on_startup=*/true);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->old_title, u"Source App");
  EXPECT_EQ(result->new_title, std::nullopt);
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(result->old_icon.AsBitmap(), cyan_bitmap));
  EXPECT_FALSE(result->new_icon.has_value());
  EXPECT_EQ(result->old_start_url, GURL(kSourceAppUrl));
  EXPECT_EQ(result->new_start_url, GURL(kTargetAppUrl));
  EXPECT_TRUE(result->is_forced_migration);
  EXPECT_FALSE(result->icon_diff_is_insignificant);
}

}  // namespace web_app
