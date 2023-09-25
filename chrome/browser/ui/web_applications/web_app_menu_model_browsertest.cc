// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace web_app {

class TestWebAppMenuModelCR2023 : public WebAppControllerBrowserTest {
 public:
  TestWebAppMenuModelCR2023()
      : WebAppControllerBrowserTest({features::kChromeRefresh2023}, {}) {}

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

}  // namespace web_app
