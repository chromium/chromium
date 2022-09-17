// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <memory>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

void CheckShortcut(const ui::SimpleMenuModel& model,
                   size_t index,
                   int shortcut_index,
                   const std::u16string& label,
                   absl::optional<SkColor> color) {
  EXPECT_EQ(model.GetTypeAt(index), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetCommandIdAt(index),
            ash::LAUNCH_APP_SHORTCUT_FIRST + shortcut_index);
  EXPECT_EQ(model.GetLabelAt(index), label);

  ui::ImageModel icon = model.GetIconAt(index);
  if (color.has_value()) {
    EXPECT_FALSE(icon.GetImage().IsEmpty());
    EXPECT_EQ(icon.GetImage().AsImageSkia().bitmap()->getColor(15, 15), color);
  } else {
    EXPECT_TRUE(icon.IsEmpty());
  }
}

void CheckSeparator(const ui::SimpleMenuModel& model, size_t index) {
  EXPECT_EQ(model.GetTypeAt(index), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(model.GetCommandIdAt(index), -1);
}

}  // namespace

using WebAppsChromeOsBrowserTest = web_app::WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppsChromeOsBrowserTest, ShortcutIcons) {
  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const web_app::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), app_url);
  LaunchWebAppBrowser(app_id);

  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  {
    ash::ShelfModel* const shelf_model = ash::ShelfModel::Get();
    PinAppWithIDToShelf(app_id);
    ash::ShelfItemDelegate* const delegate =
        shelf_model->GetShelfItemDelegate(ash::ShelfID(app_id));
    base::RunLoop run_loop;
    delegate->GetContextMenu(
        display::Display::GetDefaultDisplay().id(),
        base::BindLambdaForTesting(
            [&run_loop,
             &menu_model](std::unique_ptr<ui::SimpleMenuModel> model) {
              menu_model = std::move(model);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Shortcuts appear last in the context menu.
  // See /web_app_shortcuts/shortcuts.json for shortcut icon definitions.
  size_t index = menu_model->GetItemCount() - 11;

  // Purpose |any| by default.
  CheckShortcut(*menu_model, index++, 0, u"One", SK_ColorGREEN);
  CheckSeparator(*menu_model, index++);
  // Purpose |maskable| takes precedence over |any|.
  CheckShortcut(*menu_model, index++, 1, u"Two", SK_ColorBLUE);
  CheckSeparator(*menu_model, index++);
  // Purpose |any|.
  CheckShortcut(*menu_model, index++, 2, u"Three", SK_ColorYELLOW);
  CheckSeparator(*menu_model, index++);
  // Purpose |any| and |maskable|.
  CheckShortcut(*menu_model, index++, 3, u"Four", SK_ColorCYAN);
  CheckSeparator(*menu_model, index++);
  // Purpose |maskable|.
  CheckShortcut(*menu_model, index++, 4, u"Five", SK_ColorMAGENTA);
  CheckSeparator(*menu_model, index++);
  // No icons.
  CheckShortcut(*menu_model, index++, 5, u"Six", absl::nullopt);
  EXPECT_EQ(index, menu_model->GetItemCount());

  const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + 3;
  ui_test_utils::UrlLoadObserver url_observer(
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html#four"),
      content::NotificationService::AllSources());
  menu_model->ActivatedAt(menu_model->GetIndexOfCommandId(command_id).value(),
                          ui::EF_LEFT_MOUSE_BUTTON);
  url_observer.Wait();
}
