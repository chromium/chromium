// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {

using ::testing::Eq;
using ::testing::IsFalse;

namespace {

class ShortcutSubManagerTestBase : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const int kTotalIconSizes = 9;

  ShortcutSubManagerTestBase() = default;
  ~ShortcutSubManagerTestBase() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ = OsIntegrationTestOverrideImpl::OverrideForTesting();
    }
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    fake_provider().SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  webapps::AppId InstallWebAppWithShortcuts(
      std::map<SquareSizePx, SkBitmap> icon_map,
      bool skip_trusted_icons = false) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->icon_bitmaps.any = icon_map;
    if (!skip_trusted_icons) {
      info->trusted_icon_bitmaps.any = icon_map;
    }
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    fake_provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

 protected:
  SkBitmap CreateSolidColorIcon(int size, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size, size);
    bitmap.eraseColor(color);
    return bitmap;
  }

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

class ShortcutSubManagerConfigureTest
    : public ShortcutSubManagerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ShortcutSubManagerConfigureTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kWebAppUsePrimaryIcon);
    } else {
      feature_list_.InitAndDisableFeature(features::kWebAppUsePrimaryIcon);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ShortcutSubManagerConfigureTest, ConfigureAppInstall) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  icon_map[icon_size::k512] =
      CreateSolidColorIcon(icon_size::k512, SK_ColorYELLOW);
  const webapps::AppId& app_id = InstallWebAppWithShortcuts(icon_map);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  ASSERT_THAT(state.value().shortcut().title(), testing::Eq("Test App"));

  int icons_count = GetParam() ? icon_map.size() : kTotalIconSizes;
  ASSERT_THAT(state.value().shortcut().icon_data_any_size(),
              testing::Eq(icons_count));

  for (const proto::os_state::ShortcutIconData& icon_time_map_data :
       state.value().shortcut().icon_data_any()) {
    ASSERT_THAT(
        syncer::ProtoTimeToTime(icon_time_map_data.timestamp()).is_null(),
        testing::IsFalse());
  }
}

TEST_P(ShortcutSubManagerConfigureTest, FallbackToManifestIconsNoTrusted) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  icon_map[icon_size::k512] =
      CreateSolidColorIcon(icon_size::k512, SK_ColorYELLOW);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(icon_map, /*skip_trusted_icons=*/true);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  // Instead of the specific trusted icons of 2 sizes being used, the test will
  // use all possible sizes that are used by the manifest icons (AKA
  // web_app::SizesToGenerate() + one size of k512).
  ASSERT_THAT(state.value().shortcut().title(), testing::Eq("Test App"));
  EXPECT_EQ(7, state.value().shortcut().icon_data_any_size());

  for (const proto::os_state::ShortcutIconData& icon_time_map_data :
       state.value().shortcut().icon_data_any()) {
    ASSERT_THAT(
        syncer::ProtoTimeToTime(icon_time_map_data.timestamp()).is_null(),
        testing::IsFalse());
  }
}

TEST_P(ShortcutSubManagerConfigureTest, ConfigureAppUninstall) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  icon_map[icon_size::k512] =
      CreateSolidColorIcon(icon_size::k512, SK_ColorYELLOW);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map));

  test::UninstallAllWebApps(profile());
  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_FALSE(state.has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ShortcutSubManagerConfigureTest,
                         ::testing::Bool(),
                         [](::testing::TestParamInfo<bool> info) {
                           return info.param ? "TrustedIconsOn"
                                             : "TrustedIconsOff";
                         });

class ShortcutSubManagerExecuteTest
    : public ShortcutSubManagerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ShortcutSubManagerExecuteTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kWebAppUsePrimaryIcon);
    } else {
      feature_list_.InitAndDisableFeature(features::kWebAppUsePrimaryIcon);
    }
  }

  bool HasShortcutsOsIntegration() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    return true;
#else
    return false;
#endif
  }

  SkColor GetShortcutColor(const webapps::AppId& app_id,
                           const std::string& app_name) {
    if (!HasShortcutsOsIntegration()) {
      return SK_ColorTRANSPARENT;
    }

    scoped_refptr<OsIntegrationTestOverrideImpl> test_override =
        OsIntegrationTestOverrideImpl::Get();

#if BUILDFLAG(IS_WIN)
    std::optional<SkColor> application_menu_icon_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->application_menu(), app_id, app_name);
    if (!application_menu_icon_color) {
      ADD_FAILURE() << "Could not get shortcut icon color";
      return SK_ColorTRANSPARENT;
    }
    return application_menu_icon_color.value();
#elif BUILDFLAG(IS_MAC)
    std::optional<SkColor> icon_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->chrome_apps_folder(), app_id, app_name);
    if (!icon_color) {
      ADD_FAILURE() << "Could not get shortcut icon color";
      return SK_ColorTRANSPARENT;
    }
    return icon_color.value();
#elif BUILDFLAG(IS_LINUX)
    std::optional<SkColor> icon_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->desktop(), app_id, app_name,
            kLauncherIconSize);
    if (!icon_color) {
      ADD_FAILURE() << "Could not get shortcut icon color";
      return SK_ColorTRANSPARENT;
    }
    return icon_color.value();
#else
    NOTREACHED() << "Shortcuts not supported for other OS";
#endif
  }

  webapps::AppId UpdateInstalledWebAppWithNewIcons(
      std::map<SquareSizePx, SkBitmap> updated_icons) {
    std::unique_ptr<WebAppInstallInfo> updated_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    updated_info->title = u"New App";
    updated_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    updated_info->icon_bitmaps.any = updated_icons;
    updated_info->trusted_icon_bitmaps.any = updated_icons;

    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        update_future;
    fake_provider().install_finalizer().FinalizeUpdate(
        *updated_info, update_future.GetCallback());
    bool success = update_future.Wait();
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(update_future.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return update_future.Get<webapps::AppId>();
  }

  webapps::AppId InstallWebAppNoIntegration(
      std::map<SquareSizePx, SkBitmap> icon_map) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->icon_bitmaps.any = icon_map;
    info->trusted_icon_bitmaps.any = icon_map;

    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    // InstallFromInfoNoIntegrationForTesting() does not trigger OS integration.
    fake_provider().scheduler().InstallFromInfoNoIntegrationForTesting(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ShortcutSubManagerExecuteTest, InstallAppVerifyCorrectShortcuts) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map));

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id,
        fake_provider().registrar_unsafe().GetAppShortName(app_id)));

    // On all desktop platforms, the shortcut icon that is used for the
    // launcher is icon_size::k128, which should be GREEN as per the icon_map
    // being used above.
    EXPECT_THAT(
        GetShortcutColor(
            app_id, fake_provider().registrar_unsafe().GetAppShortName(app_id)),
        testing::Eq(SK_ColorGREEN));
  }
}

TEST_P(ShortcutSubManagerExecuteTest, UpdateAppVerifyCorrectShortcuts) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorYELLOW);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map));

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id,
        fake_provider().registrar_unsafe().GetAppShortName(app_id)));
    EXPECT_THAT(
        GetShortcutColor(
            app_id, fake_provider().registrar_unsafe().GetAppShortName(app_id)),
        testing::Eq(SK_ColorYELLOW));
  }

  std::map<SquareSizePx, SkBitmap> updated_icon_map;
  updated_icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorBLUE);
  const webapps::AppId& updated_app_id =
      UpdateInstalledWebAppWithNewIcons(std::move(updated_icon_map));
  EXPECT_EQ(updated_app_id, app_id);

  auto updated_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          updated_app_id);
  ASSERT_TRUE(updated_state.has_value());

  if (HasShortcutsOsIntegration()) {
    // Verify shortcut changes for both name and color.
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id,
        fake_provider().registrar_unsafe().GetAppShortName(app_id)));
    EXPECT_THAT(
        GetShortcutColor(
            app_id, fake_provider().registrar_unsafe().GetAppShortName(app_id)),
        testing::Eq(SK_ColorBLUE));
  }
}

TEST_P(ShortcutSubManagerExecuteTest,
       TwoConsecutiveInstallsUpdateShortcutLocations) {
  // Install an app with icons but no shortcuts.
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  const webapps::AppId& app_id =
      InstallWebAppNoIntegration(std::move(icon_map));

  std::string app_name =
      fake_provider().registrar_unsafe().GetAppShortName(app_id);

  // Call synchronize with empty options to set up the current_states, but
  // without any shortcut locations defined.
  test::SynchronizeOsIntegration(profile(), app_id, SynchronizeOsOptions());

  auto os_integration_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(os_integration_state.has_value());
  EXPECT_FALSE(os_integration_state->has_shortcut());

  if (HasShortcutsOsIntegration()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
  }

  // This should trigger the application to become fully installed.
  base::test::TestFuture<void> future;
  fake_provider().scheduler().SetUserDisplayMode(
      app_id, mojom::UserDisplayMode::kStandalone, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  os_integration_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(os_integration_state.has_value());
  EXPECT_TRUE(os_integration_state->has_shortcut());

  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
    EXPECT_THAT(
        GetShortcutColor(
            app_id, fake_provider().registrar_unsafe().GetAppShortName(app_id)),
        testing::Eq(SK_ColorRED));
  }

  // Mimic a 2nd installation with updated icons so that the update flow gets
  // triggered.
  std::map<SquareSizePx, SkBitmap> new_icons;
  new_icons[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorYELLOW);
  const webapps::AppId& expected_app_id =
      InstallWebAppWithShortcuts(std::move(new_icons));
  ASSERT_EQ(expected_app_id, app_id);

  os_integration_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          expected_app_id);
  ASSERT_TRUE(os_integration_state.has_value());

  // Shortcuts should be created now.
  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(os_integration_state->has_shortcut());
// TODO(crbug.com/339024222): The color doesn't correctly update to yellow on
// windows.
#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), expected_app_id,
        fake_provider().registrar_unsafe().GetAppShortName(expected_app_id)));
    EXPECT_THAT(GetShortcutColor(expected_app_id, app_name),
                testing::Eq(SK_ColorYELLOW));
#endif
  }
}

TEST_P(ShortcutSubManagerExecuteTest, UninstallAppRemovesShortcuts) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] =
      CreateSolidColorIcon(icon_size::k16, SK_ColorYELLOW);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map));

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id,
        fake_provider().registrar_unsafe().GetAppShortName(app_id)));
    EXPECT_THAT(
        GetShortcutColor(
            app_id, fake_provider().registrar_unsafe().GetAppShortName(app_id)),
        testing::Eq(SK_ColorRED));
  }

  test::UninstallAllWebApps(profile());
  if (HasShortcutsOsIntegration()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id,
        fake_provider().registrar_unsafe().GetAppShortName(app_id)));
  }
}

TEST_P(ShortcutSubManagerExecuteTest, ForceUnregisterAppInRegistry) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map));
  const std::string& app_name =
      fake_provider().registrar_unsafe().GetAppShortName(app_id);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
  }

  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);

  if (HasShortcutsOsIntegration()) {
    ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
  }
}

TEST_P(ShortcutSubManagerExecuteTest, ForceUnregisterAppNotInRegistry) {
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map));
  const std::string& app_name =
      fake_provider().registrar_unsafe().GetAppShortName(app_id);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          app_id);
  ASSERT_TRUE(state.has_value());

  if (HasShortcutsOsIntegration()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
  }

  test::UninstallAllWebApps(profile());
  if (HasShortcutsOsIntegration()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
  }
  EXPECT_FALSE(fake_provider().registrar_unsafe().IsInRegistrar(app_id));

  // Force unregister shouldn't change anything.
  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);
  if (HasShortcutsOsIntegration()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id, app_name));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ShortcutSubManagerExecuteTest,
                         ::testing::Bool(),
                         [](::testing::TestParamInfo<bool> info) {
                           return info.param ? "TrustedIconsOn"
                                             : "TrustedIconsOff";
                         });

}  // namespace
}  // namespace web_app
