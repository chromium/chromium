// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class ShortcutMenuHandlingSubManagerTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  ShortcutMenuHandlingSubManagerTest() = default;
  ~ShortcutMenuHandlingSubManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverride::OverrideForTesting(base::GetHomeDir());
    }
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }

    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), /*icon_manager=*/nullptr, file_handler_manager.get(),
        protocol_handler_manager.get());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), /*url_handler_manager=*/nullptr);

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

  web_app::AppId InstallWebAppWithShortcutMenuIcons(
      ShortcutsMenuIconBitmaps shortcuts_menu_icons) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->shortcuts_menu_icon_bitmaps = shortcuts_menu_icons;
    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      test_override_;
};

TEST_P(ShortcutMenuHandlingSubManagerTest, TestConfigure) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    EXPECT_TRUE(
        os_integration_state.shortcut_menus().shortcut_menu_info_size() ==
        num_menu_items);

    EXPECT_TRUE(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(0)
                    .app_title() == "Test App");
    EXPECT_TRUE(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(0)
                    .app_launch_url() == "https://example.com/path/index.html");

    int num_sizes = static_cast<int>(sizes.size());

    for (int menu_index = 0; menu_index < num_menu_items; menu_index++) {
      EXPECT_TRUE(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .icon_data_any_size() == num_sizes);
      EXPECT_TRUE(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .icon_data_maskable_size() == num_sizes);
      EXPECT_TRUE(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .icon_data_monochrome_size() == num_sizes);

      for (int size_index = 0; size_index < num_sizes; size_index++) {
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_any(size_index)
                        .icon_size() == sizes[size_index]);
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_any(size_index)
                        .has_timestamp());
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_maskable(size_index)
                        .icon_size() == sizes[size_index]);
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_maskable(size_index)
                        .has_timestamp());
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_monochrome(size_index)
                        .icon_size() == sizes[size_index]);
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_monochrome(size_index)
                        .has_timestamp());
      }
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ShortcutMenuHandlingSubManagerTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
