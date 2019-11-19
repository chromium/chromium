// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/simple_menu_model.h"

class AppMenuIconController;
class BookmarkSubMenuModel;
class Browser;

namespace {
class MockAppMenuModel;
}  // namespace

// Values should correspond to 'WrenchMenuAction' enum in histograms.xml.
enum AppMenuAction {
  MENU_ACTION_NEW_TAB = 0,
  MENU_ACTION_NEW_WINDOW = 1,
  MENU_ACTION_NEW_INCOGNITO_WINDOW = 2,
  MENU_ACTION_SHOW_BOOKMARK_BAR = 3,
  MENU_ACTION_SHOW_BOOKMARK_MANAGER = 4,
  MENU_ACTION_IMPORT_SETTINGS = 5,
  MENU_ACTION_BOOKMARK_THIS_TAB = 6,
  MENU_ACTION_BOOKMARK_ALL_TABS = 7,
  MENU_ACTION_PIN_TO_START_SCREEN = 8,
  MENU_ACTION_RESTORE_TAB = 9,
  MENU_ACTION_DISTILL_PAGE = 13,
  MENU_ACTION_SAVE_PAGE = 14,
  MENU_ACTION_FIND = 15,
  MENU_ACTION_PRINT = 16,
  MENU_ACTION_CUT = 17,
  MENU_ACTION_COPY = 18,
  MENU_ACTION_PASTE = 19,
  MENU_ACTION_CREATE_HOSTED_APP = 20,
  MENU_ACTION_MANAGE_EXTENSIONS = 22,
  MENU_ACTION_TASK_MANAGER = 23,
  MENU_ACTION_CLEAR_BROWSING_DATA = 24,
  MENU_ACTION_VIEW_SOURCE = 25,
  MENU_ACTION_DEV_TOOLS = 26,
  MENU_ACTION_DEV_TOOLS_CONSOLE = 27,
  MENU_ACTION_DEV_TOOLS_DEVICES = 28,
  MENU_ACTION_PROFILING_ENABLED = 29,
  MENU_ACTION_ZOOM_MINUS = 30,
  MENU_ACTION_ZOOM_PLUS = 31,
  MENU_ACTION_FULLSCREEN = 32,
  MENU_ACTION_SHOW_HISTORY = 33,
  MENU_ACTION_SHOW_DOWNLOADS = 34,
  MENU_ACTION_SHOW_SYNC_SETUP = 35,
  MENU_ACTION_OPTIONS = 36,
  MENU_ACTION_ABOUT = 37,
  MENU_ACTION_HELP_PAGE_VIA_MENU = 38,
  MENU_ACTION_FEEDBACK = 39,
  MENU_ACTION_TOGGLE_REQUEST_TABLET_SITE = 40,
  MENU_ACTION_EXIT = 43,
  MENU_ACTION_RECENT_TAB = 41,
  MENU_ACTION_BOOKMARK_OPEN = 42,
  MENU_ACTION_UPGRADE_DIALOG = 44,
  MENU_ACTION_CAST = 45,
  MENU_ACTION_BETA_FORUM = 46,
  MENU_ACTION_COPY_URL = 47,
  MENU_ACTION_OPEN_IN_CHROME = 48,
  MENU_ACTION_SITE_SETTINGS = 49,
  MENU_ACTION_APP_INFO = 50,
  LIMIT_MENU_ACTION
};

// Function to record WrenchMenu.MenuAction histogram
void LogWrenchMenuAction(AppMenuAction action_id);

// A menu model that builds the contents of the zoom menu.
class ZoomMenuModel : public ui::SimpleMenuModel {
 public:
  explicit ZoomMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  ~ZoomMenuModel() override;

 private:
  void Build();

  DISALLOW_COPY_AND_ASSIGN(ZoomMenuModel);
};

class ToolsMenuModel : public ui::SimpleMenuModel {
 public:
  ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate, Browser* browser);
  ~ToolsMenuModel() override;

 private:
  void Build(Browser* browser);

  DISALLOW_COPY_AND_ASSIGN(ToolsMenuModel);
};

// A menu model that builds the contents of the app menu.
class AppMenuModel : public ui::SimpleMenuModel,
                     public ui::SimpleMenuModel::Delegate,
                     public ui::ButtonMenuItemModel::Delegate,
                     public TabStripModelObserver,
                     public content::WebContentsObserver {
 public:
  // Range of command IDs to use for the items in the recent tabs submenu.
  static const int kMinRecentTabsCommandId = 1001;
  static const int kMaxRecentTabsCommandId = 1200;

  // Creates an app menu model for the given browser. Init() must be called
  // before passing this to an AppMenu. |app_menu_icon_controller|, if provided,
  // is used to decide whether or not to include an item for opening the upgrade
  // dialog.
  AppMenuModel(ui::AcceleratorProvider* provider,
               Browser* browser,
               AppMenuIconController* app_menu_icon_controller = nullptr);
  ~AppMenuModel() override;

  // Runs Build() and registers observers.
  void Init();

  // Overridden for ButtonMenuItemModel::Delegate:
  bool DoesCommandIdDismissMenu(int command_id) const override;

  // Overridden for both ButtonMenuItemModel::Delegate and SimpleMenuModel:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // Getters.
  Browser* browser() const { return browser_; }

  BookmarkSubMenuModel* bookmark_sub_menu_model() const {
    return bookmark_sub_menu_model_.get();
  }

  // Calculates |zoom_label_| in response to a zoom change.
  void UpdateZoomControls();

 protected:
  // Helper function to record the menu action in a UMA histogram.
  virtual void LogMenuAction(AppMenuAction action_id);

  // Builds the menu model, adding appropriate menu items.
  virtual void Build();

  // Appends a clipboard menu (without separators).
  void CreateCutCopyPasteMenu();

  // Add a menu item for the browser action icons if there is overflow, returns
  // whether the menu was added.
  bool CreateActionToolbarOverflowMenu();

  // Appends a zoom menu (without separators).
  void CreateZoomMenu();

 private:
  friend class ::MockAppMenuModel;

  bool ShouldShowNewIncognitoWindowMenuItem();

  // Adds actionable global error menu items to the menu.
  // Examples: Extension permissions and sign in errors.
  // Returns a boolean indicating whether any menu items were added.
  bool AddGlobalErrorMenuItems();

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  // Called when a command is selected.
  // Logs UMA metrics about which command was chosen and how long the user
  // took to select the command.
  void LogMenuMetrics(int command_id);

  // Time menu has been open. Used by LogMenuMetrics() to record the time
  // to action when the user selects a menu item.
  base::ElapsedTimer timer_;

  // Whether a UMA menu action has been recorded since the menu is open.
  // Only the first time to action is recorded since some commands
  // (zoom controls) don't dimiss the menu.
  bool uma_action_recorded_;

  // Models for the special menu items with buttons.
  std::unique_ptr<ui::ButtonMenuItemModel> edit_menu_item_model_;
  std::unique_ptr<ui::ButtonMenuItemModel> zoom_menu_item_model_;

  // Label of the zoom label in the zoom menu item.
  base::string16 zoom_label_;

  // Bookmark submenu.
  std::unique_ptr<BookmarkSubMenuModel> bookmark_sub_menu_model_;

  // Other submenus.
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> sub_menus_;

  ui::AcceleratorProvider* provider_;  // weak

  Browser* const browser_;  // weak
  AppMenuIconController* const app_menu_icon_controller_;

  std::unique_ptr<content::HostZoomMap::Subscription>
      browser_zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_
