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
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/simple_menu_model.h"

class AppMenuIconController;
class BookmarkSubMenuModel;
class Browser;
class ChromeLabsModel;

// Values should correspond to 'WrenchMenuAction' enum in enums.xml.
//
// LINT.IfChange(AppMenuAction)
enum AppMenuAction {
  MENU_ACTION_NEW_TAB = 0,
  MENU_ACTION_NEW_WINDOW = 1,
  MENU_ACTION_NEW_INCOGNITO_WINDOW = 2,
  MENU_ACTION_SHOW_BOOKMARK_BAR = 3,
  MENU_ACTION_SHOW_BOOKMARK_MANAGER = 4,
  MENU_ACTION_IMPORT_SETTINGS = 5,
  MENU_ACTION_BOOKMARK_THIS_TAB = 6,
  MENU_ACTION_BOOKMARK_ALL_TABS = 7,
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
  MENU_ACTION_UNINSTALL_APP = 51,
  MENU_ACTION_CHROME_TIPS = 53,
  MENU_ACTION_CHROME_WHATS_NEW = 54,
  MENU_ACTION_LACROS_DATA_MIGRATION = 55,
  MENU_ACTION_MENU_OPENED = 56,
  MENU_ACTION_VISIT_CHROME_WEB_STORE = 57,
  MENU_ACTION_PASSWORD_MANAGER = 58,
  MENU_ACTION_SHOW_TRANSLATE = 59,
  MENU_ACTION_SHOW_CHROME_LABS = 60,
  MENU_ACTION_INSTALL_PWA = 61,
  MENU_ACTION_OPEN_IN_PWA_WINDOW = 62,
  MENU_ACTION_SEND_TO_DEVICES = 63,
  MENU_ACTION_CREATE_QR_CODE = 64,
  MENU_ACTION_CUSTOMIZE_CHROME = 65,
  MENU_ACTION_CLOSE_PROFILE = 66,
  MENU_ACTION_MANAGE_GOOGLE_ACCOUNT = 67,
  MENU_SHOW_SIGNIN_WHEN_PAUSED = 68,
  MENU_SHOW_SYNC_SETTINGS = 69,
  MENU_TURN_ON_SYNC = 70,
  MENU_ACTION_OPEN_GUEST_PROFILE = 71,
  MENU_ACTION_ADD_NEW_PROFILE = 72,
  MENU_ACTION_MANAGE_CHROME_PROFILES = 73,
  MENU_ACTION_RECENT_TABS_LOGIN_FOR_DEVICE_TABS = 74,
  MENU_ACTION_READING_LIST_ADD_TAB = 75,
  MENU_ACTION_READING_LIST_SHOW_UI = 76,
  MENU_ACTION_SHOW_PASSWORD_MANAGER = 77,
  MENU_ACTION_SHOW_PAYMENT_METHODS = 78,
  MENU_ACTION_SHOW_ADDRESSES = 79,
  MENU_ACTION_SWITCH_TO_ANOTHER_PROFILE = 80,
  MENU_ACTION_SHOW_SEARCH_COMPANION = 81,
  MENU_ACTION_SHOW_BOOKMARK_SIDE_PANEL = 82,
  MENU_ACTION_SHOW_PERFORMANCE_SETTINGS = 83,
  MENU_ACTION_SHOW_HISTORY_CLUSTER_SIDE_PANEL = 84,
  MENU_ACTION_SHOW_READING_MODE_SIDE_PANEL = 85,
  MENU_ACTION_SHOW_SAFETY_HUB = 86,
  MENU_ACTION_SAFETY_HUB_SHOW_PASSWORD_CHECKUP = 87,
  MENU_ACTION_SET_BROWSER_AS_DEFAULT = 88,
  MENU_ACTION_SHOW_SAVED_TAB_GROUPS = 89,
  MENU_ACTION_SHOW_LENS_OVERLAY = 90,
  MENU_ACTION_SAFETY_HUB_MANAGE_EXTENSIONS = 91,
  MENU_ACTION_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL = 92,
  LIMIT_MENU_ACTION
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:WrenchMenuAction)

enum class AlertMenuItem { kNone, kPasswordManager };

// Function to record WrenchMenu.MenuAction histogram
void LogWrenchMenuAction(AppMenuAction action_id);

// Given the menu model and command_id, set the icon to the given vector-icon.
// This is a no-op if the command is unavailable.
void SetCommandIcon(ui::SimpleMenuModel* model,
                    int command_id,
                    const gfx::VectorIcon& vector_icon);

class ToolsMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPerformanceMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChromeLabsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kReadingModeMenuItem);

  ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate, Browser* browser);

  ToolsMenuModel(const ToolsMenuModel&) = delete;
  ToolsMenuModel& operator=(const ToolsMenuModel&) = delete;

  ~ToolsMenuModel() override;

 private:
  void Build(Browser* browser);

  std::unique_ptr<ChromeLabsModel> chrome_labs_model_;
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

// A menu model that builds the contents of the app menu.
class AppMenuModel : public ui::SimpleMenuModel,
                     public ui::SimpleMenuModel::Delegate,
                     public ui::ButtonMenuItemModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kProfileMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kProfileOpenGuestItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBookmarksMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabGroupsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDownloadsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHistoryMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExtensionsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMoreToolsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIncognitoMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordAndAutofillMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordManagerMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kShowLensOverlay);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kShowSearchCompanion);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSaveAndShareMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCastTitleItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInstallAppItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateShortcutItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSetBrowserAsDefaultMenuItem);

  // Number of menus within the app menu with an arbitrarily high (variable)
  // number of menu items. For example, the number of bookmarks menu items
  // varies depending upon the underlying model. The command IDs for items in
  // these menus will be staggered and each increment by this value, so they
  // don't have conflicts. Currently, this accounts for the bookmarks, recent
  // tabs menus, the profile submenu and tab groups submenu.
  static constexpr int kNumUnboundedMenuTypes = 4;

  // First command ID to use for each unbounded menu. These should be staggered,
  // and there should be kNumUnboundedMenuTypes of them.
  static constexpr int kMinBookmarksCommandId = IDC_FIRST_UNBOUNDED_MENU;
  static constexpr int kMinRecentTabsCommandId = kMinBookmarksCommandId + 1;
  static constexpr int kMinOtherProfileCommandId = kMinRecentTabsCommandId + 1;
  static constexpr int kMinTabGroupsCommandId = kMinOtherProfileCommandId + 1;

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

  void SetHighlightedIdentifier(
      ui::ElementIdentifier highlighted_menu_identifier);

  // Overridden for ButtonMenuItemModel::Delegate:
  bool DoesCommandIdDismissMenu(int command_id) const override;

  // Overridden for both ButtonMenuItemModel::Delegate and SimpleMenuModel:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdAlerted(int command_id) const override;
  bool IsElementIdAlerted(ui::ElementIdentifier element_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // Getters.
  Browser* browser() const { return browser_; }

  BookmarkSubMenuModel* bookmark_sub_menu_model() const {
    return bookmark_sub_menu_model_.get();
  }

 protected:
  // Helper function to record the menu action in a UMA histogram.
  virtual void LogMenuAction(AppMenuAction action_id);

  // Builds the menu model, adding appropriate menu items.
  virtual void Build();

  // Appends a clipboard menu (without separators).
  void CreateCutCopyPasteMenu();

  // Appends a Find and edit sub-menu (without separators)
  void CreateFindAndEditSubMenu();

  // Appends a zoom menu (without separators).
  void CreateZoomMenu();

  // Called when a command is selected.
  // Logs UMA metrics about which command was chosen and how long the user
  // took to select the command.
  void LogMenuMetrics(int command_id);

  // Logs UMA metrics when the user interacted with a Safety Hub notification
  // in the menu. When an expected module is provided, the metrics will only be
  // logged when the module matches the one for which there is an active menu
  // notification.
  void LogSafetyHubInteractionMetrics(safety_hub::SafetyHubModuleType sh_module,
                                      int event_flags);

 private:
  // Adds actionable global error menu items to the menu.
  // Examples: Extension permissions and sign in errors.
  // Returns a boolean indicating whether any menu items were added.
  bool AddGlobalErrorMenuItems();

  // Adds actionable default browser prompt menu items to the menu. Returns a
  // boolean indicating whether any menu items were added.
  bool AddDefaultBrowserMenuItems();

  // Adds the Safety Hub menu notifications to the menu. Returns a boolean
  // indicating whether any menu items were added.
  [[nodiscard]] bool AddSafetyHubMenuItem();

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

  // Bookmark submenu.
  std::unique_ptr<BookmarkSubMenuModel> bookmark_sub_menu_model_;

  // Other submenus.
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> sub_menus_;

  raw_ptr<ui::AcceleratorProvider> provider_;  // weak

  const raw_ptr<Browser> browser_;  // weak
  const raw_ptr<AppMenuIconController> app_menu_icon_controller_;

  PrefChangeRegistrar local_state_pref_change_registrar_;

  const AlertMenuItem alert_item_;

  ui::ElementIdentifier highlighted_menu_identifier_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_MODEL_H_
