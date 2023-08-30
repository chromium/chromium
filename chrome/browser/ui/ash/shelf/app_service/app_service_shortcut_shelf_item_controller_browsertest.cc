// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shortcut_shelf_item_controller.h"

#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"

class AppServiceShortcutShelfItemControllerBrowserTest
    : public InProcessBrowserTest {
 protected:
  AppServiceShortcutShelfItemControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCrosWebAppShortcutUiUpdate);
  }

  void SetUpOnMainThread() override { ASSERT_TRUE(controller()); }

  apps::ShortcutId CreateWebAppBasedShortcut(
      const GURL& app_url,
      const std::u16string& shortcut_name) {
    // Create web app based shortcut.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->title = shortcut_name;
    auto local_shortcut_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    return apps::GenerateShortcutId(app_constants::kChromeAppId,
                                    local_shortcut_id);
  }

  ChromeShelfController* controller() {
    return ChromeShelfController::instance();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServiceShortcutShelfItemControllerBrowserTest,
                       SelectItem) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  PinAppWithIDToShelf(shortcut_id.value());

  ash::ShelfItemDelegate* delegate =
      controller()->shelf_model()->GetShelfItemDelegate(
          ash::ShelfID(shortcut_id.value()));

  ASSERT_TRUE(delegate);

  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  delegate->ItemSelected(/*event=*/nullptr, display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN,
                         /*callback=*/base::DoNothing(),
                         /*filter_predicate=*/base::NullCallback());
  url_observer.Wait();
}
