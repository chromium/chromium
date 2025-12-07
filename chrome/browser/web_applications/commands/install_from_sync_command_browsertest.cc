// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include <optional>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_observer_test_utils.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {
namespace {

constexpr int kIconSize = 96;

class InstallFromSyncCommandTest : public base::test::WithFeatureOverride,
                                   public WebAppBrowserTestBase {
 public:
  InstallFromSyncCommandTest()
      : base::test::WithFeatureOverride{features::kWebAppUsePrimaryIcon} {}
  ~InstallFromSyncCommandTest() override = default;

  GURL GetManifestIcon() {
    return https_server()->GetURL("/banners/launcher-icon-2x.png");
  }

  GURL GetTrustedIcon() {
    return https_server()->GetURL("/banners/image-512px.png");
  }

  base::FilePath LoadImageFile() {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &path);
    if (GetParam()) {
      // This corresponds to the largest icon in
      // chrome/test/data/banners/manifest.json, which will be used on ChromeOS
      // always if trusted icons are enabled.
      return path.AppendASCII("banners").Append(
          FILE_PATH_LITERAL("image-512px.png"));
    } else {
      return path.AppendASCII("banners").Append(
          FILE_PATH_LITERAL("launcher-icon-2x.png"));
    }
  }

  // Get the primary icon for `app_id` of `size` from the disk.
  SkBitmap GetAppIconOfSize(const webapps::AppId& app_id, int size) {
    WebAppProvider* web_app_provider = WebAppProvider::GetForTest(profile());
    base::test::TestFuture<SizeToBitmap> test_future;
    web_app_provider->icon_manager().ReadIconAndResize(
        app_id, IconPurpose::ANY, size, test_future.GetCallback());
    SizeToBitmap bitmaps = test_future.Take();
    CHECK(base::Contains(bitmaps, size));
    return bitmaps[size];
  }

  // Read the expected icon from the image file that will be set according to
  // `GetParam()`.
  SkBitmap GetExpectedIconFromDisk(int size) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::optional<std::vector<uint8_t>> file_contents =
        base::ReadFileToBytes(LoadImageFile());
    CHECK(file_contents.has_value());
    SkBitmap png_bytes = gfx::PNGCodec::Decode(file_contents.value());
    CHECK(!png_bytes.empty());
    if (png_bytes.width() == size) {
      return png_bytes;
    }

    return skia::ImageOperations::Resize(
        png_bytes, skia::ImageOperations::RESIZE_BEST, size, size);
  }
};

IN_PROC_BROWSER_TEST_P(InstallFromSyncCommandTest, SimpleInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  webapps::AppId id = GenerateAppId(std::nullopt, test_url);

  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      id, GenerateManifestIdFromStartUrlOnly(test_url), /*start_url=*/test_url,
      "Test Title",
      /*scope=*/https_server()->GetURL("/banners/"),
      /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone,
      {apps::IconInfo(GetManifestIcon(), kIconSize)},
      {apps::IconInfo(GetTrustedIcon(), kIconSize)});
  provider->command_manager().ScheduleCommand(
      std::make_unique<InstallFromSyncCommand>(
          profile(), params,
          base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_EQ(app_id, id);
            loop.Quit();
          })));
  loop.Run();
  EXPECT_EQ(provider->registrar_unsafe().GetInstallState(id),
            AreAppsLocallyInstalledBySync()
                ? proto::INSTALLED_WITH_OS_INTEGRATION
                : proto::SUGGESTED_FROM_ANOTHER_DEVICE);

  EXPECT_TRUE(gfx::test::AreBitmapsClose(GetExpectedIconFromDisk(kIconSize),
                                         GetAppIconOfSize(id, kIconSize),
                                         /*max_deviation=*/3));
}

IN_PROC_BROWSER_TEST_P(InstallFromSyncCommandTest, TwoInstalls) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  webapps::AppId id = GenerateAppId(std::nullopt, test_url);
  GURL other_test_url = https_server()->GetURL(
      "/banners/"
      "manifest_no_service_worker.html");
  webapps::AppId other_id = GenerateAppId(std::nullopt, other_test_url);
  const int other_icon_size = 64;

  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  {
    InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
        id, GenerateManifestIdFromStartUrlOnly(test_url),
        /*start_url=*/test_url, "Test Title",
        /*scope=*/https_server()->GetURL("/banners/"),
        /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone,
        {apps::IconInfo(GetManifestIcon(), kIconSize)},
        {apps::IconInfo(GetTrustedIcon(), kIconSize)});
    provider->command_manager().ScheduleCommand(
        std::make_unique<InstallFromSyncCommand>(
            profile(), params,
            base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                           webapps::InstallResultCode code) {
              EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              EXPECT_EQ(app_id, id);
            })));
  }
  {
    InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
        other_id, GenerateManifestIdFromStartUrlOnly(other_test_url),
        /*start_url=*/other_test_url, "Test Title",
        /*scope=*/https_server()->GetURL("/banners/"),
        /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone,
        {apps::IconInfo(GetManifestIcon(), other_icon_size)},
        {apps::IconInfo(GetTrustedIcon(), other_icon_size)});
    provider->command_manager().ScheduleCommand(
        std::make_unique<InstallFromSyncCommand>(
            profile(), params,
            base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                           webapps::InstallResultCode code) {
              EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              EXPECT_EQ(app_id, other_id);
              loop.Quit();
            })));
  }
  loop.Run();
  // Check first install.
  {
    EXPECT_EQ(provider->registrar_unsafe().GetInstallState(id),
              AreAppsLocallyInstalledBySync()
                  ? proto::INSTALLED_WITH_OS_INTEGRATION
                  : proto::SUGGESTED_FROM_ANOTHER_DEVICE);

    EXPECT_TRUE(gfx::test::AreBitmapsClose(GetExpectedIconFromDisk(kIconSize),
                                           GetAppIconOfSize(id, kIconSize),
                                           /*max_deviation=*/3));
  }
  // Check second install.
  {
    EXPECT_EQ(provider->registrar_unsafe().GetInstallState(other_id),
              AreAppsLocallyInstalledBySync()
                  ? proto::INSTALLED_WITH_OS_INTEGRATION
                  : proto::SUGGESTED_FROM_ANOTHER_DEVICE);

    EXPECT_TRUE(
        gfx::test::AreBitmapsClose(GetExpectedIconFromDisk(other_icon_size),
                                   GetAppIconOfSize(other_id, other_icon_size),
                                   /*max_deviation=*/3));
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(InstallFromSyncCommandTest);

}  // namespace
}  // namespace web_app
