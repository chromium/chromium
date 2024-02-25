// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SHORTCUT_SHELF_ITEM_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class Browser;
class ShelfContextMenu;

// Item controller for an app shortcut.
// If the associated app is a platform or ARC app, launching the app replaces
// this instance with an AppWindowShelfItemController to handle the app's
// windows. Closing all associated AppWindows will replace that delegate with
// a new instance of this class (if the app is pinned to the shelf).
//
// Non-platform app types do not use AppWindows. This delegate is not replaced
// when browser windows are opened for those app types.
class AppShortcutShelfItemController : public ash::ShelfItemDelegate,
                                       public BrowserListObserver {
 public:
  explicit AppShortcutShelfItemController(const ash::ShelfID& shelf_id);

  AppShortcutShelfItemController(const AppShortcutShelfItemController&) =
      delete;
  AppShortcutShelfItemController& operator=(
      const AppShortcutShelfItemController&) = delete;

  ~AppShortcutShelfItemController() override;

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  AppMenuItems GetAppMenuItems(
      int event_flags,
      const ItemFilterPredicate& filter_predicate) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // Get the refocus url pattern, which can be used to identify this application
  // from a URL link.
  const GURL& refocus_url() const { return refocus_url_; }
  // Set the refocus url pattern. Used by unit tests.
  void set_refocus_url(const GURL& refocus_url) { refocus_url_ = refocus_url; }

  bool HasRunningApplications();

 private:
  // BrowserListObserver:
  void OnBrowserClosing(Browser* browser) override;

  // |filter_predicate| is used to filter out the app webcontents and app
  // browsers results based on their corresponding windows.
  std::vector<raw_ptr<content::WebContents, VectorExperimental>>
  GetAppWebContents(const ItemFilterPredicate& filter_predicate);
  std::vector<raw_ptr<Browser, VectorExperimental>> GetAppBrowsers(
      const ItemFilterPredicate& filter_predicate);

  // If an owned item is already active, this function advances to the next item
  // (or bounce the browser if there is only one item) and returns a shelf
  // action. Otherwise, it returns nullopt.
  // |filter_predicate| is used to filter out unwanted options to advance to
  // based on their corresponding windows.
  std::optional<ash::ShelfAction> AdvanceToNextApp(
      const ItemFilterPredicate& filter_predicate);

  // Returns true if the application is a V2 app.
  bool IsV2App();

  // Returns true if it is allowed to try starting a V2 app again.
  bool AllowNextLaunchAttempt();

  bool IsWindowedWebApp();

  size_t AppMenuSize();
  void ClearAppMenu();

  GURL refocus_url_;

  // Since V2 applications can be undetectable after launching, this timer is
  // keeping track of the last launch attempt.
  base::Time last_launch_attempt_;

  // The cached lists of open app shown in an application menu. We either cache
  // by the web contents or by the browsers, and this is indicated by the value
  // of |app_menu_cached_by_browsers_|.
  std::vector<raw_ptr<content::WebContents, VectorExperimental>>
      app_menu_web_contents_;
  std::vector<raw_ptr<Browser, VectorExperimental>> app_menu_browsers_;
  bool app_menu_cached_by_browsers_ = false;

  std::unique_ptr<ShelfContextMenu> context_menu_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
