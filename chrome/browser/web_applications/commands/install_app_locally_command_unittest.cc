// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_app_locally_command.h"

#include <map>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class InstallAppLocallyCommandTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  InstallAppLocallyCommandTest() = default;
  ~InstallAppLocallyCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ = OsIntegrationTestOverrideImpl::OverrideForTesting();
    }
    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
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

  webapps::AppId InstallNonLocallyInstalledAppWithIcons(
      std::map<SquareSizePx, SkBitmap> icon_map) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    info->icon_bitmaps.any = std::move(icon_map);
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;

    web_app::WebAppInstallParams params;
    params.install_state = proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
    params.add_to_search = false;
    // InstallFromInfo does not trigger OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), params);
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    const webapps::AppId app_id = result.Get<webapps::AppId>();
    return app_id;
  }

  bool HasShortcutsOsIntegration() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    return true;
#else
    return false;
#endif
  }

 protected:
  WebAppProvider& provider() { return *provider_; }
  SkBitmap CreateSolidColorIcon(int size, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size, size);
    bitmap.eraseColor(color);
    return bitmap;
  }

  SkColor GetShortcutColor(const webapps::AppId& app_id,
                           const std::string& app_name) {
    if (!HasShortcutsOsIntegration()) {
      return SK_ColorTRANSPARENT;
    }

    scoped_refptr<OsIntegrationTestOverrideImpl> test_override =
        OsIntegrationTestOverrideImpl::Get();

#if BUILDFLAG(IS_WIN)
    std::optional<SkColor> desktop_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->desktop(), app_id, app_name);
    std::optional<SkColor> application_menu_icon_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->application_menu(), app_id, app_name);
    EXPECT_EQ(desktop_color.value(), application_menu_icon_color.value());
    return desktop_color.value();
#elif BUILDFLAG(IS_MAC)
    std::optional<SkColor> icon_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->chrome_apps_folder(), app_id, app_name);
    EXPECT_TRUE(icon_color.has_value());
    return icon_color.value();
#elif BUILDFLAG(IS_LINUX)
    std::optional<SkColor> icon_color =
        test_override->GetShortcutIconTopLeftColor(
            profile(), test_override->desktop(), app_id, app_name,
            kLauncherIconSize);
    EXPECT_TRUE(icon_color.has_value());
    return icon_color.value();
#else
    NOTREACHED_IN_MIGRATION() << "Shortcuts not supported for other OS";
    return SK_ColorTRANSPARENT;
#endif
  }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

TEST_F(InstallAppLocallyCommandTest, BasicBehavior) {
  // Create an app that is not locally installed, i.e. has the
  // is_locally_installed bit set to false and there is no OS integration
  // defined for it.
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallNonLocallyInstalledAppWithIcons(std::move(icon_map));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (HasShortcutsOsIntegration()) {
    ASSERT_FALSE(os_integration_state.has_shortcut());
  }

  // Install app locally.
  base::test::TestFuture<void> test_future;
  provider().scheduler().InstallAppLocally(app_id, test_future.GetCallback());
  EXPECT_TRUE(test_future.Wait());

  auto updated_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_state.has_value());
  const proto::WebAppOsIntegrationState& updated_os_states =
      updated_state.value();
  ASSERT_TRUE(updated_os_states.has_shortcut());

  // OS integration should be triggered now.
  if (HasShortcutsOsIntegration()) {
    ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id)));

    // On all desktop platforms, the shortcut icon that is used for the
    // launcher is icon_size::k128, which should be GREEN as per the icon_map
    // being used above.
    ASSERT_THAT(
        GetShortcutColor(app_id,
                         provider().registrar_unsafe().GetAppShortName(app_id)),
        testing::Eq(SK_ColorGREEN));
  }
}

TEST_F(InstallAppLocallyCommandTest, AppNotInRegistrar) {
  const webapps::AppId app_id = "abcde";

  base::test::TestFuture<void> test_future;
  provider().scheduler().InstallAppLocally(app_id, test_future.GetCallback());
  EXPECT_TRUE(test_future.Wait());
  EXPECT_FALSE(provider().registrar_unsafe().IsInstallState(
      app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               proto::INSTALLED_WITH_OS_INTEGRATION}));
}

}  // namespace
}  // namespace web_app
