// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/chrome_labs_model.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/simple_menu_model.h"

class AppMenuIconController;
class BookmarkSubMenuModel;
class Browser;

// Values should correspond to 'WrenchMenuAction' enum in enums.xml.
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
  // Only used by WebAppMenuModel:
  MENU_ACTION_UNINSTALL_APP = 51,
  MENU_ACTION_CHROME_TIPS = 53,
  MENU_ACTION_CHROME_WHATS_NEW = 54,
  MENU_ACTION_LACROS_DATA_MIGRATION = 55,
  MENU_ACTION_MENU_OPENED = 56,
  // Only used by ExtensionsMenuModel sub menu.
  MENU_ACTION_VISIT_CHROME_WEB_STORE = 57,
  MENU_ACTION_PASSWORD_MANAGER = 58,
  MENU_ACTION_TRANSLATE_PAGE = 59,
  // ToolsMenuModel
  MENU_ACTION_SHOW_CHROME_LABS = 60,
  LIMIT_MENU_ACTION
};

enum class AlertMenuItem { kNone, kReopenTabs, kPerformance };

// Function to record WrenchMenu.MenuAction histogram
void LogWrenchMenuAction(AppMenuAction action_id);

// A menu model that builds the contents of the zoom menu.
class ZoomMenuModel : public ui::SimpleMenuModel {
 public:
  explicit ZoomMenuModel(ui::SimpleMenuModel::Delegate* delegate);

  ZoomMenuModel(const ZoomMenuModel&) = delete;
  ZoomMenuModel& operator=(const ZoomMenuModel&) = delete;

  ~ZoomMenuModel() override;

 private:
  void Build();
};

class ToolsMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPerformanceMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChromeLabsMenuItem);

  ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate, Browser* browser);

  ToolsMenuModel(const ToolsMenuModel&) = delete;
  ToolsMenuModel& operator=(const ToolsMenuModel&) = delete;

  ~ToolsMenuModel() override;

 private:
  void Build(Browser* browser);

  std::unique_ptr<ChromeLabsModel> chrome_labs_model_ = nullptr;
};

class ExtensionsMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kManageExtensionsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kVisitChromeWebStoreMenuItem);

  ExtensionsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                      Browser* browser);

  ExtensionsMenuModel(const ExtensionsMenuModel&) = delete;
  ExtensionsMenuModel& operator=(const ExtensionsMenuModel&) = delete;

  ~ExtensionsMenuModel() override;

 private:
  void Build(Browser* browser);
};

class PasswordsAndAutofillSubMenuModel : public ui::SimpleMenuModel {
 public:
  explicit PasswordsAndAutofillSubMenuModel(
      ui::SimpleMenuModel::Delegate* delegate);

  PasswordsAndAutofillSubMenuModel(const PasswordsAndAutofillSubMenuModel&) =
      delete;
  PasswordsAndAutofillSubMenuModel& operator=(
      const PasswordsAndAutofillSubMenuModel&) = delete;

  ~PasswordsAndAutofillSubMenuModel() override;
};

class FindAndEditSubMenuModel : public ui::SimpleMenuModel {
 public:
  explicit FindAndEditSubMenuModel(ui::SimpleMenuModel::Delegate* delegate);

  FindAndEditSubMenuModel(const FindAndEditSubMenuModel&) = delete;
  FindAndEditSubMenuModel& operator=(const FindAndEditSubMenuModel&) = delete;

  ~FindAndEditSubMenuModel() override;
};

// A menu model that builds the contents of the app menu.
class AppMenuModel : public ui::SimpleMenuModel,
                     public ui::SimpleMenuModel::Delegate,
                     public ui::ButtonMenuItemModel::Delegate,
                     public TabStripModelObserver,
                     public content::WebContentsObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBookmarksMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDownloadsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHistoryMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExtensionsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMoreToolsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIncognitoMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordManagerMenuItem);

  // First command ID to use for the recent tabs menu. This is one higher than
  // the first command id used for the bookmarks menus, as the command ids for
  // these menus should be offset to avoid conflicts.
  static const int kMinRecentTabsCommandId = IDC_FIRST_UNBOUNDED_MENU + 1;
  // Number of menus within the app menu with an arbitrarily high (variable)
  // number of menu items. For example, the number of bookmarks menu items
  // varies depending upon the underlying model. Currently, this accounts for
  // the bookmarks and recent tabs menus.
  static const int kNumUnboundedMenuTypes = 2;

  // Creates an app menu model for the given browser. Init() must be called
  // before passing this to an AppMenu. |app_menu_icon_controller|, if provided,
  // is used to decide whether or not to include an item for opening the upgrade
  // dialog.
  AppMenuModel(ui::AcceleratorProvider* provider,
               Browser* browser,
               AppMenuIconController* app_menu_icon_controller = nullptr,
               AlertMenuItem alert_item = AlertMenuItem::kNone);

  AppMenuModel(const AppMenuModel&) = delete;
  AppMenuModel& operator=(const AppMenuModel&) = delete;

  ~AppMenuModel() override;

  // Runs Build() and registers observers.
  void Init();

  // Overridden for ButtonMenuItemModel::Delegate:
  bool DoesCommandIdDismissMenu(int command_id) const override;

  // Overridden for both ButtonMenuItemModel::Delegate and SimpleMenuModel:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsCommandIdAlerted(int command_id) const override;
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

  // Appends a zoom menu (without separators).
  void CreateZoomMenu();

 private:
  // Adds actionable global error menu items to the menu.
  // Examples: Extension permissions and sign in errors.
  // Returns a boolean indicating whether any menu items were added.
  bool AddGlobalErrorMenuItems();

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  // Called when a command is selected.
  // Logs UMA metrics about which command was chosen and how long the user
  // took to select the command.
  void LogMenuMetrics(int command_id);

#if BUILDFLAG(IS_CHROMEOS)
  // Disables/Enables the settings item based on kSystemFeaturesDisableList
  // pref.
  void UpdateSettingsItemState();
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  std::u16string zoom_label_;

  // Bookmark submenu.
  std::unique_ptr<BookmarkSubMenuModel> bookmark_sub_menu_model_;

  // Other submenus.
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> sub_menus_;

  raw_ptr<ui::AcceleratorProvider> provider_;  // weak

  const raw_ptr<Browser> browser_;  // weak
  const raw_ptr<AppMenuIconController> app_menu_icon_controller_;

  base::CallbackListSubscription browser_zoom_subscription_;

  PrefChangeRegistrar local_state_pref_change_registrar_;

  const AlertMenuItem alert_item_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_
