// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/generated_icon_fix_command.h"

#include <memory>

#include "base/check_deref.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/scheduler/generated_icon_fix_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {
namespace {
using ::testing::Contains;
using ::testing::Pair;

class GeneratedIconFixCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  webapps::AppId InstallAppWithGeneratedIcon() {
    auto install_info = std::make_unique<WebAppInstallInfo>(
        web_app::GenerateManifestIdFromStartUrlOnly(
            GURL("https://example.com/app/")),
        GURL("https://example.com/app/"));
    install_info->title = u"Test App";

    webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(install_info));
    CHECK(
        provider().registrar_unsafe().GetAppById(app_id)->is_generated_icon());
    return app_id;
  }
};

TEST_F(GeneratedIconFixCommandTest, Success) {
  webapps::AppId app_id = InstallAppWithGeneratedIcon();

  // Verify it has a generated icon.
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->is_generated_icon());

  const GURL kIconUrl("https://example.com/app/icon.png");

  // Update the app to have a manifest icon URL.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id);
    app->SetManifestIcons({apps::IconInfo(kIconUrl, 144)});
  }

  // Set up the fake icon downloader to return a real icon.
  auto& icon_state = fake_web_contents_manager().GetOrCreateIconState(kIconUrl);
  icon_state.bitmaps = {gfx::test::CreateBitmap(144, SK_ColorGREEN)};

  base::test::TestFuture<GeneratedIconFixResult> future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<GeneratedIconFixCommand>(
          app_id, proto::GENERATED_ICON_FIX_SOURCE_RETROACTIVE,
          future.GetCallback()));

  EXPECT_EQ(future.Get(), GeneratedIconFixResult::kSuccess);

  // Verify the icon is no longer generated.
  EXPECT_FALSE(
      provider().registrar_unsafe().GetAppById(app_id)->is_generated_icon());

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> bitmaps_future;
  provider().icon_manager().ReadAllIcons(app_id, bitmaps_future.GetCallback());
  ASSERT_TRUE(bitmaps_future.Wait());

  EXPECT_THAT(
      bitmaps_future.Get().manifest_icons.any,
      Contains(Pair(144, gfx::test::EqualsBitmap(icon_state.bitmaps[0]))));
}

TEST_F(GeneratedIconFixCommandTest, AppUninstalled) {
  webapps::AppId app_id = InstallAppWithGeneratedIcon();

  // Uninstall the app before the command runs.
  test::UninstallWebApp(profile(), app_id);

  base::test::TestFuture<GeneratedIconFixResult> future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<GeneratedIconFixCommand>(
          app_id, proto::GENERATED_ICON_FIX_SOURCE_RETROACTIVE,
          future.GetCallback()));

  EXPECT_EQ(future.Get(), GeneratedIconFixResult::kAppUninstalled);
}

TEST_F(GeneratedIconFixCommandTest, DownloadFailure) {
  webapps::AppId app_id = InstallAppWithGeneratedIcon();

  const GURL kIconUrl("https://example.com/app/icon.png");

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id);
    app->SetManifestIcons({apps::IconInfo(kIconUrl, 144)});
  }

  // Set up the fake icon downloader to fail.
  auto& icon_state = fake_web_contents_manager().GetOrCreateIconState(kIconUrl);
  icon_state.http_status_code = 404;
  icon_state.trigger_primary_page_changed_if_fetched = true;

  base::test::TestFuture<GeneratedIconFixResult> future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<GeneratedIconFixCommand>(
          app_id, proto::GENERATED_ICON_FIX_SOURCE_RETROACTIVE,
          future.GetCallback()));

  EXPECT_EQ(future.Get(), GeneratedIconFixResult::kDownloadFailure);
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->is_generated_icon());
}

TEST_F(GeneratedIconFixCommandTest, StillGenerated) {
  webapps::AppId app_id = InstallAppWithGeneratedIcon();

  const GURL kIconUrl("https://example.com/app/icon.png");

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id);
    app->SetManifestIcons({apps::IconInfo(kIconUrl, 144)});
  }

  // Downloader returns nothing, so PopulateProductIcons will still have
  // is_generated_icon = true.
  auto& icon_state = fake_web_contents_manager().GetOrCreateIconState(kIconUrl);
  icon_state.bitmaps = {};

  base::test::TestFuture<GeneratedIconFixResult> future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<GeneratedIconFixCommand>(
          app_id, proto::GENERATED_ICON_FIX_SOURCE_RETROACTIVE,
          future.GetCallback()));

  EXPECT_EQ(future.Get(), GeneratedIconFixResult::kStillGenerated);
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->is_generated_icon());
}

TEST_F(GeneratedIconFixCommandTest, FixSourceRecorded) {
  webapps::AppId app_id = InstallAppWithGeneratedIcon();

  base::test::TestFuture<GeneratedIconFixResult> future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<GeneratedIconFixCommand>(
          app_id, proto::GENERATED_ICON_FIX_SOURCE_RETROACTIVE,
          future.GetCallback()));

  ASSERT_TRUE(future.Wait());

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app->generated_icon_fix().has_value());
  EXPECT_EQ(app->generated_icon_fix()->source(),
            proto::GENERATED_ICON_FIX_SOURCE_RETROACTIVE);
}

}  // namespace
}  // namespace web_app
