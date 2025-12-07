// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_menu_model.h"

#include <algorithm>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace web_app {

class TestWebAppMenuModelCR2023 : public WebAppBrowserTestBase {
 public:
  TestWebAppMenuModelCR2023() : WebAppBrowserTestBase({}, {}) {}

  TestWebAppMenuModelCR2023(const TestWebAppMenuModelCR2023&) = delete;
  TestWebAppMenuModelCR2023& operator=(const TestWebAppMenuModelCR2023&) =
      delete;

  ~TestWebAppMenuModelCR2023() override = default;
};

IN_PROC_BROWSER_TEST_F(TestWebAppMenuModelCR2023, ModelHasIcons) {
  const GURL app_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);

  const auto check_for_icons = [](std::u16string menu_name,
                                  ui::MenuModel* model) -> void {
    auto check_for_icons_impl = [](std::u16string menu_name,
                                   ui::MenuModel* model,
                                   auto& check_for_icons_ref) -> void {
      // Except where noted by the above vector, all menu items in CR2023 must
      // have icons.
      for (size_t i = 0; i < model->GetItemCount(); ++i) {
        auto menu_type = model->GetTypeAt(i);
        if (menu_type != ui::MenuModel::TYPE_ACTIONABLE_SUBMENU &&
            menu_type != ui::MenuModel::TYPE_SUBMENU) {
          continue;
        }
        if (menu_type != ui::MenuModel::TYPE_SEPARATOR &&
            menu_type != ui::MenuModel::TYPE_TITLE) {
          EXPECT_TRUE(!model->GetIconAt(i).IsEmpty())
              << "\"" << menu_name << "\" menu item \"" << model->GetLabelAt(i)
              << "\" is missing the icon!";
        }
        if ((menu_type == ui::MenuModel::TYPE_SUBMENU ||
             menu_type == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU)) {
          check_for_icons_ref(model->GetLabelAt(i), model->GetSubmenuModelAt(i),
                              check_for_icons_ref);
        }
      }
    };
    check_for_icons_impl(menu_name, model, check_for_icons_impl);
  };

  {
    WebAppMenuModel model(nullptr, browser);
    model.Init();
    check_for_icons(u"<Root Menu>", &model);
  }

  UninstallWebApp(app_id);
}

IN_PROC_BROWSER_TEST_F(TestWebAppMenuModelCR2023, CommandStatusTest) {
  const GURL app_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);

  {
    WebAppMenuModel model(nullptr, browser);
    model.Init();
    EXPECT_TRUE(
        model.IsCommandIdEnabled(WebAppMenuModel::kUninstallAppCommandId));
    EXPECT_TRUE(model.IsCommandIdEnabled(IDC_COPY_URL));
    EXPECT_TRUE(model.IsCommandIdEnabled(IDC_PRINT));
  }

  UninstallWebApp(app_id);
}

class WebAppMenuModelBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppMenuModelBrowserTest()
      : WebAppBrowserTestBase({features::kWebAppPredictableAppUpdating}, {}) {}
  ~WebAppMenuModelBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebAppMenuModelBrowserTest, HasPendingUpdate) {
  const GURL app_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);

  {
    WebAppMenuModel app_menu_model(nullptr, browser);
    app_menu_model.Init();

    // Verify that "Review update" button is not visible in the menu.
    ui::MenuModel* model = &app_menu_model;
    size_t index = 0;
    const bool found = ui::MenuModel::GetModelAndIndexForCommandId(
        IDC_WEB_APP_UPGRADE_DIALOG, &model, &index);
    EXPECT_FALSE(found);
  }

  // Set the `was_ignored` field to true deliberately to ensure that the menu
  // model still works correctly even if the user has ignored an update.
  {
    web_app::ScopedRegistryUpdate update =
        provider().sync_bridge_unsafe().BeginUpdate();
    web_app::proto::PendingUpdateInfo update_info;
    update_info.set_name("Updated app name");
    update_info.set_was_ignored(true);
    update->UpdateApp(app_id)->SetPendingUpdateInfo(std::move(update_info));
  }

  {
    WebAppMenuModel app_menu_model(nullptr, browser);
    app_menu_model.Init();

    // Verify that "Review update" button is visible in the menu.
    ui::MenuModel* model = &app_menu_model;
    size_t index = 0;
    const bool found = ui::MenuModel::GetModelAndIndexForCommandId(
        IDC_WEB_APP_UPGRADE_DIALOG, &model, &index);
    EXPECT_TRUE(found);
    EXPECT_TRUE(app_menu_model.IsCommandIdEnabled(IDC_WEB_APP_UPGRADE_DIALOG));
    EXPECT_TRUE(model->IsEnabledAt(index));
    EXPECT_TRUE(app_menu_model.IsCommandIdVisible(IDC_WEB_APP_UPGRADE_DIALOG));
    EXPECT_TRUE(model->IsVisibleAt(index));
    ui::ImageModel update_icon = model->GetIconAt(index);
    ASSERT_TRUE(update_icon.IsImage());
    EXPECT_EQ(update_icon.Size().width(), update_icon.Size().height());
    EXPECT_EQ(update_icon.Size().width(),
              ui::SimpleMenuModel::kDefaultIconSize);
    EXPECT_TRUE(gfx::test::AreImagesClose(
        update_icon.GetImage(),
        gfx::Image(provider().icon_manager().GetFaviconImageSkia(app_id)),
        /*max_deviation=*/1));
  }

  UninstallWebApp(app_id);
}

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

using WebAppModelMenuPreventCloseTest = PreventCloseTestBase;

IN_PROC_BROWSER_TEST_F(WebAppModelMenuPreventCloseTest,
                       PreventCloseEnforedByPolicy) {
  InstallPWA(GURL(kCalculatorAppUrl), ash::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(ash::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  Browser* const browser =
      LaunchPWA(ash::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  {
    auto app_menu_model =
        std::make_unique<WebAppMenuModel>(/*provider=*/nullptr, browser);
    app_menu_model->Init();

    // Verify that "Open in Chrome" button is not visible in the menu.
    ui::MenuModel* model = app_menu_model.get();
    size_t index = 0;
    const bool found = ui::MenuModel::GetModelAndIndexForCommandId(
        IDC_OPEN_IN_CHROME, &model, &index);
    EXPECT_EQ(!kShouldPreventClose, found);
  }

  test::UninstallAllWebApps(browser->profile());
}

}  // namespace web_app
