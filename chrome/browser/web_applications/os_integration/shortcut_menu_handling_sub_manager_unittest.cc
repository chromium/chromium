// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
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
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu_win.h"
#endif

using ::testing::ElementsAreArray;
using ::testing::Eq;

namespace web_app {

namespace {

class ShortcutMenuHandlingSubManagerTestBase : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  ShortcutMenuHandlingSubManagerTestBase() = default;
  ~ShortcutMenuHandlingSubManagerTestBase() override = default;

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

  webapps::AppId InstallWebAppWithShortcutMenuIcons(
      ShortcutsMenuIconBitmaps shortcuts_menu_icons) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->shortcuts_menu_icon_bitmaps = shortcuts_menu_icons;
    info->shortcuts_menu_item_infos =
        CreateShortcutMenuItemInfoFromBitmaps(shortcuts_menu_icons);
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
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
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

// Synchronize tests only. Tests here should only verify DB updates.
using ShortcutMenuHandlingSubManagerConfigureTest =
    ShortcutMenuHandlingSubManagerTestBase;

TEST_F(ShortcutMenuHandlingSubManagerConfigureTest, TestConfigure) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    EXPECT_TRUE(
        os_integration_state.shortcut_menus().shortcut_menu_info_size() ==
        num_menu_items);

    int num_sizes = static_cast<int>(sizes.size());

    for (int menu_index = 0; menu_index < num_menu_items; menu_index++) {
      EXPECT_THAT(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .shortcut_name(),
                  testing::Eq(base::StrCat(
                      {"shortcut_name", base::NumberToString(menu_index)})));

      EXPECT_THAT(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .shortcut_launch_url(),
                  testing::Eq(base::StrCat(
                      {kWebAppUrl.spec(), base::NumberToString(menu_index)})));

      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_any_size(),
                num_sizes);
      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_maskable_size(),
                num_sizes);
      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_monochrome_size(),
                num_sizes);

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
}

// Tests handling crashes fixed in crbug.com/1417955.
TEST_F(ShortcutMenuHandlingSubManagerConfigureTest, IconsButNoShortcutInfo) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  // Remove the shortcut menu item infos from the DB and sync OS integration.
  {
    ScopedRegistryUpdate remove_downloaded =
        provider().sync_bridge_unsafe().BeginUpdate();
    remove_downloaded->UpdateApp(app_id)->SetShortcutsMenuInfo({});
  }
  test::SynchronizeOsIntegration(profile(), app_id);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  ASSERT_FALSE(os_integration_state.has_shortcut_menus());
}

// Tests handling crashes fixed in crbug.com/1417955.
TEST_F(ShortcutMenuHandlingSubManagerConfigureTest,
       LessShortcutMenuItemsThanIconInfos) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  auto icon_bitmaps =
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcutMenuIcons(icon_bitmaps);

  // Create a single WebAppShortcutsMenuItemInfo.
  WebAppShortcutsMenuItemInfo shortcut_info;
  shortcut_info.name = base::UTF8ToUTF16(base::StrCat({"basic_shortcut"}));
  shortcut_info.url = kWebAppUrl;

  // The URLs used do not matter because Execute() does not take the urls
  // into account, but we still need those to initialize the mock data
  // structure so that the GURL checks in WebAppDatabase can pass.
  for (const auto& [size, data] : icon_bitmaps[0].any) {
    WebAppShortcutsMenuItemInfo::Icon icon_data;
    icon_data.square_size_px = size;
    icon_data.url = GURL("https://icon.any/");
    shortcut_info.any.push_back(std::move(icon_data));
  }

  for (const auto& [size, data] : icon_bitmaps[0].maskable) {
    WebAppShortcutsMenuItemInfo::Icon icon_data;
    icon_data.square_size_px = size;
    icon_data.url = GURL("https://icon.maskable/");
    shortcut_info.maskable.push_back(std::move(icon_data));
  }

  for (const auto& [size, data] : icon_bitmaps[0].monochrome) {
    WebAppShortcutsMenuItemInfo::Icon icon_data;
    icon_data.square_size_px = size;
    icon_data.url = GURL("https://icon.monochrome/");
    shortcut_info.monochrome.push_back(std::move(icon_data));
  }

  shortcut_info.downloaded_icon_sizes.any = sizes;
  shortcut_info.downloaded_icon_sizes.maskable = sizes;
  shortcut_info.downloaded_icon_sizes.monochrome = sizes;

  // Update the shortcut menu item infos in the DB to only match a single icon
  // and rerun OS integration.
  {
    ScopedRegistryUpdate remove_downloaded =
        provider().sync_bridge_unsafe().BeginUpdate();
    remove_downloaded->UpdateApp(app_id)->SetShortcutsMenuInfo({shortcut_info});
  }

    test::SynchronizeOsIntegration(profile(), app_id);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    ASSERT_EQ(os_integration_state.shortcut_menus().shortcut_menu_info_size(),
              1);

    auto shortcut_menu_info =
        os_integration_state.shortcut_menus().shortcut_menu_info(0);
    EXPECT_EQ(shortcut_menu_info.shortcut_name(), "basic_shortcut");
    EXPECT_EQ(shortcut_menu_info.shortcut_launch_url(), kWebAppUrl.spec());
    EXPECT_EQ(shortcut_menu_info.icon_data_any_size(), 2);
    EXPECT_EQ(shortcut_menu_info.icon_data_maskable_size(), 2);
    EXPECT_EQ(shortcut_menu_info.icon_data_monochrome_size(), 2);
}

// This tests our handling of https://crbug.com/1427444.
TEST_F(ShortcutMenuHandlingSubManagerConfigureTest, NoDownloadedIcons_1427444) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));
  // Remove the downloaded icons & resync os integration.
  {
    ScopedRegistryUpdate remove_downloaded =
        provider().sync_bridge_unsafe().BeginUpdate();
    remove_downloaded->UpdateApp(app_id)->SetShortcutsMenuInfo({});
  }
    test::SynchronizeOsIntegration(profile(), app_id);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  ASSERT_FALSE(os_integration_state.has_shortcut_menus());
}

// Synchronize and Execute tests from here onwards. Tests here should
// verify both DB updates as well as OS registrations/unregistrations.
using ShortcutMenuHandlingSubManagerExecuteTest =
    ShortcutMenuHandlingSubManagerTestBase;

TEST_F(ShortcutMenuHandlingSubManagerExecuteTest, InstallWritesCorrectData) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            app_user_model_id),
        testing::Eq(num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            app_user_model_id),
        testing::ElementsAreArray(colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
}

TEST_F(ShortcutMenuHandlingSubManagerExecuteTest,
       EmptyDataDoesNotRegisterShortcutsMenu) {
  const webapps::AppId& app_id =
      InstallWebAppWithShortcutMenuIcons(ShortcutsMenuIconBitmaps());

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
}

TEST_F(ShortcutMenuHandlingSubManagerExecuteTest,
       UninstallRemovesShortcutMenuItems) {
  const int num_menu_items = 3;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128,
                                  icon_size::k256};
  const std::vector<SkColor> colors = {SK_ColorBLUE, SK_ColorBLUE,
                                       SK_ColorBLUE};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            app_user_model_id),
        testing::Eq(num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            app_user_model_id),
        testing::ElementsAreArray(colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif

  test::UninstallAllWebApps(profile());

#if BUILDFLAG(IS_WIN)
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  const std::wstring app_user_model_id2 =
      web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
  ASSERT_TRUE(os_integration_state.has_shortcut_menus());
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
          app_user_model_id2));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
}

TEST_F(ShortcutMenuHandlingSubManagerExecuteTest, UpdateShortcutMenuItems) {
  const int num_menu_items = 2;
  const std::vector<int> sizes = {icon_size::k32, icon_size::k48};
  const std::vector<SkColor> colors = {SK_ColorCYAN, SK_ColorCYAN};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            app_user_model_id),
        testing::Eq(num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            app_user_model_id),
        testing::ElementsAreArray(colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif

  const int updated_num_menu_items = 3;
  const std::vector<int> updated_sizes = {icon_size::k64, icon_size::k128,
                                          icon_size::k256};
  const std::vector<SkColor> updated_colors = {SK_ColorYELLOW, SK_ColorYELLOW,
                                               SK_ColorYELLOW};
  const webapps::AppId& updated_app_id =
      InstallWebAppWithShortcutMenuIcons(MakeIconBitmaps(
          {{IconPurpose::ANY, updated_sizes, updated_colors},
           {IconPurpose::MASKABLE, updated_sizes, updated_colors},
           {IconPurpose::MONOCHROME, updated_sizes, updated_colors}},
          updated_num_menu_items));
  ASSERT_THAT(updated_app_id, testing::Eq(app_id));

  state = provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
      updated_app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
    const std::wstring updated_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), updated_app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            updated_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            updated_model_id),
        testing::Eq(updated_num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            updated_model_id),
        testing::ElementsAreArray(updated_colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
}

TEST_F(ShortcutMenuHandlingSubManagerExecuteTest,
       ForceUnregisterAppInRegistry) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
  const std::wstring app_user_model_id =
      web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
  ASSERT_TRUE(
      OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
          app_user_model_id));
  ASSERT_TRUE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#else
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif  // BUILDFLAG(IS_WIN)

  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);

#if BUILDFLAG(IS_WIN)
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#else
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(ShortcutMenuHandlingSubManagerExecuteTest,
       ForceUnregisterAppNotInRegistry) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const webapps::AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

#if BUILDFLAG(IS_WIN)
  ASSERT_TRUE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#else
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif  // BUILDFLAG(IS_WIN)

  test::UninstallAllWebApps(profile());
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(app_id));

  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);
  ASSERT_FALSE(
      OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
}

}  // namespace

}  // namespace web_app
