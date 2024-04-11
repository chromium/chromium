// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_promise_app_shelf_context_menu.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"

class AppServicePromiseAppShelfContextMenuBrowserTest
    : public InProcessBrowserTest {
 public:
  AppServicePromiseAppShelfContextMenuBrowserTest() {
    scoped_feature_list_.InitWithFeatures({ash::features::kPromiseIcons}, {});
  }
  ~AppServicePromiseAppShelfContextMenuBrowserTest() override = default;

  void AddTestPromiseApp(const apps::PackageId& package_id) {
    apps::PromiseAppPtr promise_app =
        std::make_unique<apps::PromiseApp>(package_id);
    promise_app->should_show = true;
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->PromiseAppRegistryCache()
        ->OnPromiseApp(std::move(promise_app));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppShelfContextMenuBrowserTest,
                       MenuOnlyHasPin) {
  apps::PackageId package_id(apps::PackageType::kArc, "com.example.test");
  AddTestPromiseApp(package_id);

  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
  PinAppWithIDToShelf(package_id.ToString());

  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  ash::ShelfItemDelegate* delegate =
      shelf_model->GetShelfItemDelegate(ash::ShelfID(package_id.ToString()));
  base::RunLoop run_loop;
  delegate->GetContextMenu(
      display::Display::GetDefaultDisplay().id(),
      base::BindLambdaForTesting(
          [&run_loop, &menu_model](std::unique_ptr<ui::SimpleMenuModel> model) {
            menu_model = std::move(model);
            run_loop.Quit();
          }));
  run_loop.Run();

  // The context menu should only have the option to unpin from shelf.
  EXPECT_EQ(menu_model->GetItemCount(), 1u);
  EXPECT_EQ(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu_model->GetCommandIdAt(0), ash::CommandId::TOGGLE_PIN);
}
