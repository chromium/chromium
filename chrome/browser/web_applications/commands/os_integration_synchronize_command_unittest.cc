// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"

#include <map>
#include <memory>

#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::Eq;

class OsIntegrationSynchronizeCommandTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kWebAppUrl = GURL("https://example.com");

  OsIntegrationSynchronizeCommandTest() {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers, {{"stage", "write_config"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kOsIntegrationSubManagers});
    }
  }

  ~OsIntegrationSynchronizeCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverrideImpl::OverrideForTesting(base::GetHomeDir());
    }

    provider_ = FakeWebAppProvider::Get(profile());
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    EXPECT_TRUE(test::UninstallAllWebApps(profile()));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  AppId InstallWebApp(std::unique_ptr<WebAppInstallInfo> install_info,
                      webapps::WebappInstallSource source =
                          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON) {
    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    provider()->scheduler().InstallFromInfo(
        std::move(install_info), /*overwrite_existing_manifest_fields=*/true,
        source, result.GetCallback());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

  void RunSynchronizeCommand(const AppId& app_id) {
    base::test::TestFuture<void> synchronize_future;
    provider()->scheduler().SynchronizeOsIntegration(
        app_id, synchronize_future.GetCallback());
    EXPECT_TRUE(synchronize_future.Wait());
  }

  bool EnableRunOnOsLoginMode(const AppId& app_id) {
    base::test::TestFuture<void> future;
    provider()->scheduler().SetRunOnOsLoginMode(
        app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
    return future.Wait();
  }

 protected:
  WebAppProvider* provider() { return provider_; }

  ShortcutsMenuIconBitmaps MakeIconBitmaps(
      const std::vector<GeneratedIconsInfo>& icons_info,
      int num_menu_items) {
    ShortcutsMenuIconBitmaps shortcuts_menu_icons;

    for (int i = 0; i < num_menu_items; ++i) {
      IconBitmaps menu_item_icon_map;
      for (const GeneratedIconsInfo& info : icons_info) {
        DCHECK_EQ(info.sizes_px.size(), info.colors.size());
        std::map<SquareSizePx, SkBitmap> generated_bitmaps;
        for (size_t j = 0; j < info.sizes_px.size(); ++j) {
          AddGeneratedIcon(&generated_bitmaps, info.sizes_px[j],
                           info.colors[j]);
        }
        menu_item_icon_map.SetBitmapsForPurpose(info.purpose,
                                                std::move(generated_bitmaps));
      }
      shortcuts_menu_icons.push_back(std::move(menu_item_icon_map));
    }

    return shortcuts_menu_icons;
  }

  std::vector<WebAppShortcutsMenuItemInfo>
  CreateShortcutMenuItemInfoFromBitmaps(
      const ShortcutsMenuIconBitmaps& menu_bitmaps) {
    std::vector<WebAppShortcutsMenuItemInfo> item_infos;
    int index = 0;
    for (const auto& icon_bitmap : menu_bitmaps) {
      WebAppShortcutsMenuItemInfo shortcut_info;
      shortcut_info.name = base::UTF8ToUTF16(
          base::StrCat({"shortcut_name", base::NumberToString(index)}));
      shortcut_info.url =
          GURL(base::StrCat({kWebAppUrl.spec(), base::NumberToString(index)}));

      // The URLs used do not matter because Execute() does not take the urls
      // into account, but we still need those to initialize the mock data
      // structure so that the GURL checks in WebAppDatabase can pass.
      for (const auto& [size, data] : icon_bitmap.any) {
        WebAppShortcutsMenuItemInfo::Icon icon_data;
        icon_data.square_size_px = size;
        icon_data.url = GURL("https://icon.any/");
        shortcut_info.any.push_back(std::move(icon_data));
      }

      for (const auto& [size, data] : icon_bitmap.maskable) {
        WebAppShortcutsMenuItemInfo::Icon icon_data;
        icon_data.square_size_px = size;
        icon_data.url = GURL("https://icon.maskable/");
        shortcut_info.maskable.push_back(std::move(icon_data));
      }

      for (const auto& [size, data] : icon_bitmap.monochrome) {
        WebAppShortcutsMenuItemInfo::Icon icon_data;
        icon_data.square_size_px = size;
        icon_data.url = GURL("https://icon.monochrome/");
        shortcut_info.monochrome.push_back(std::move(icon_data));
      }

      item_infos.push_back(std::move(shortcut_info));
      index++;
    }
    return item_infos;
  }

  SkBitmap CreateSolidColorIcon(int size, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size, size);
    bitmap.eraseColor(color);
    return bitmap;
  }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

TEST_P(OsIntegrationSynchronizeCommandTest, ProtocolHandlersDBWrite) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->start_url = kWebAppUrl;
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";

  install_info->protocol_handlers = {protocol_handler};
  const AppId& app_id = InstallWebApp(std::move(install_info));

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_protocols_handled());

  RunSynchronizeCommand(app_id);

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers)) {
    EXPECT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(1));
    const proto::ProtocolsHandled::Protocol& protocol_handler_state =
        os_integration_state.protocols_handled().protocols(0);
    EXPECT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler.protocol));
    EXPECT_THAT(protocol_handler_state.url(),
                testing::Eq(protocol_handler.url));
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

TEST_P(OsIntegrationSynchronizeCommandTest, FileHandlersDBWrite) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->start_url = kWebAppUrl;
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  apps::FileHandlers file_handlers;
  apps::FileHandler file_handler;
  file_handler.action = GURL("https://app.site/open-foo");
  file_handler.display_name = u"Foo opener";
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foo";
    accept_entry.file_extensions.insert(".foo");
    file_handler.accept.push_back(accept_entry);
  }
  file_handlers.push_back(file_handler);

  install_info->file_handlers = file_handlers;
  const AppId& app_id = InstallWebApp(std::move(install_info));

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_file_handling());

  RunSynchronizeCommand(app_id);

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers) &&
      ShouldRegisterFileHandlersWithOs()) {
    ASSERT_TRUE(os_integration_state.has_file_handling());
    auto file_handling = os_integration_state.file_handling();
    EXPECT_EQ(file_handling.file_handlers(0).accept_size(), 1);
    EXPECT_EQ(file_handling.file_handlers(0).display_name(), "Foo opener");
    EXPECT_EQ(file_handling.file_handlers(0).action(),
              "https://app.site/open-foo");
    EXPECT_EQ(file_handling.file_handlers(0).accept(0).mimetype(),
              "application/foo");
    EXPECT_EQ(file_handling.file_handlers(0).accept(0).file_extensions_size(),
              1);
    EXPECT_EQ(file_handling.file_handlers(0).accept(0).file_extensions(0),
              ".foo");
  } else {
    ASSERT_FALSE(os_integration_state.has_file_handling());
  }
}

TEST_P(OsIntegrationSynchronizeCommandTest, RunOnOsLoginDBWrite) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->start_url = kWebAppUrl;
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  const AppId& app_id = InstallWebApp(std::move(install_info));

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_run_on_os_login());

  EnableRunOnOsLoginMode(app_id);

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers)) {
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    EXPECT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::WINDOWED));
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(OsIntegrationSynchronizeCommandTest, ShortcutsMenuDBWrite) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->start_url = kWebAppUrl;
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  // Add data for shortcuts menu registration.
  const std::vector<int> sizes = {icon_size::k64};
  const std::vector<SkColor> colors = {SK_ColorRED};
  auto shortcuts_menu_icons = MakeIconBitmaps(
      {{IconPurpose::ANY, sizes, colors}}, /*num_menu_items=*/1);
  install_info->shortcuts_menu_icon_bitmaps = shortcuts_menu_icons;
  install_info->shortcuts_menu_item_infos =
      CreateShortcutMenuItemInfoFromBitmaps(shortcuts_menu_icons);
  const AppId& app_id = InstallWebApp(std::move(install_info));

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_shortcut_menus());

  RunSynchronizeCommand(app_id);

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers)) {
    ASSERT_TRUE(os_integration_state.has_shortcut_menus());
    EXPECT_THAT(os_integration_state.shortcut_menus().shortcut_menu_info_size(),
                testing::Eq(1));
    EXPECT_THAT(
        os_integration_state.shortcut_menus()
            .shortcut_menu_info(0)
            .shortcut_name(),
        testing::Eq(base::StrCat({"shortcut_name", base::NumberToString(0)})));

    EXPECT_THAT(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(0)
                    .shortcut_launch_url(),
                testing::Eq(base::StrCat(
                    {kWebAppUrl.spec(), base::NumberToString(0)})));
    EXPECT_EQ(os_integration_state.shortcut_menus()
                  .shortcut_menu_info(0)
                  .icon_data_any_size(),
              1);
    EXPECT_EQ(os_integration_state.shortcut_menus()
                  .shortcut_menu_info(0)
                  .icon_data_any(0)
                  .icon_size(),
              icon_size::k64);
    EXPECT_TRUE(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(0)
                    .icon_data_any(0)
                    .has_timestamp());
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }
}

TEST_P(OsIntegrationSynchronizeCommandTest, ShortcutsDBWrite) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->start_url = kWebAppUrl;
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  // Add icons for shortcuts.
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k16] = CreateSolidColorIcon(icon_size::k16, SK_ColorBLUE);
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  install_info->icon_bitmaps.any = std::move(icon_map);

  const AppId& app_id = InstallWebApp(std::move(install_info));

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_shortcut());

  RunSynchronizeCommand(app_id);

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers)) {
    ASSERT_TRUE(os_integration_state.has_shortcut());
    EXPECT_THAT(os_integration_state.shortcut().title(),
                testing::Eq("Test App"));

    for (const proto::ShortcutIconData& icon_time_map_data :
         os_integration_state.shortcut().icon_data_any()) {
      EXPECT_THAT(
          syncer::ProtoTimeToTime(icon_time_map_data.timestamp()).is_null(),
          testing::IsFalse());
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut());
  }
}

TEST_P(OsIntegrationSynchronizeCommandTest, UninstallRegistrationDBWrite) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->start_url = kWebAppUrl;
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  const AppId& app_id = InstallWebApp(std::move(install_info));

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_uninstall_registration());

  RunSynchronizeCommand(app_id);

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers)) {
    ASSERT_TRUE(os_integration_state.has_uninstall_registration());
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(
        os_integration_state.uninstall_registration().registered_with_os());
#else
    EXPECT_FALSE(
        os_integration_state.uninstall_registration().registered_with_os());
#endif
  } else {
    ASSERT_FALSE(os_integration_state.has_uninstall_registration());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OsIntegrationSynchronizeCommandTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace
}  // namespace web_app
