// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_X11_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_X11_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/dbus/menu/menu.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {
class Accelerator;
}

class Browser;
class BrowserView;
struct GlobalMenuBarCommand;
class Profile;

// Controls the Mac style menu bar on Unity.
//
// Unity has an Apple-like menu bar at the top of the screen that changes
// depending on the active window.
class GlobalMenuBarX11 : public AvatarMenuObserver,
                         public BrowserListObserver,
                         public CommandObserver,
                         public history::TopSitesObserver,
                         public sessions::TabRestoreServiceObserver,
                         public ui::SimpleMenuModel::Delegate {
 public:
  GlobalMenuBarX11(BrowserView* browser_view, aura::WindowTreeHost* host);
  ~GlobalMenuBarX11() override;

  void Initialize(DbusMenu::InitializedCallback callback);

  // Creates the object path for DbusemenuServer which is attached to |xid|.
  std::string GetPath() const;

  XID xid() const { return xid_; }

 private:
  struct HistoryItem;

  // Creates a whole menu defined with |commands| and titled with the string
  // |string_id|. Then appends it to |root_menu_|.
  ui::SimpleMenuModel* BuildStaticMenu(int string_id,
                                       const GlobalMenuBarCommand* commands);

  // Creates a HistoryItem from the data in |entry|.
  std::unique_ptr<HistoryItem> HistoryItemForTab(
      const sessions::TabRestoreService::Tab& entry);

  // Creates a menu item form |item| and inserts it in |menu| at |index|.
  void AddHistoryItemToMenu(std::unique_ptr<HistoryItem> item,
                            ui::SimpleMenuModel* menu,
                            int index);

  // Sends a message off to History for data.
  void GetTopSitesData();

  // Callback to receive data requested from GetTopSitesData().
  void OnTopSitesReceived(const history::MostVisitedURLList& visited_list);

  // Updates the visibility of the bookmark bar on pref changes.
  void OnBookmarkBarVisibilityChanged();

  void RebuildProfilesMenu();

  // This will remove all menu items in |history_menu_| starting from the item
  // with command |header_command_id| up to the item with command
  // MENU_SEPARATOR, non-inclusive.  Returns the index of the separator.
  int ClearHistoryMenuSection(int header_command_id);

  // Start listening for enabled state changes for |command|.
  void RegisterCommandObserver(int command);

  // Returns a command ID for use in menus.  The command will not conflict with
  // Chrome commands or reserved commands.
  int NextCommandId();

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // history::TopSitesObserver:
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  // TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // State for the browser window we're tracking.
  Browser* const browser_;
  Profile* profile_;
  BrowserView* browser_view_;
  XID xid_;

  // Has Initialize() been called?
  bool initialized_ = false;

  // The DBus menu service.
  std::unique_ptr<DbusMenu> menu_service_;

  // Menu models.  Menus don't own their children, so we must own them.
  // |toplevel_menus_| are children of |root_menu_|.
  // |recently_closed_window_menus_| are children of |history_menu_|.
  // |history_menu_| and |profiles_menu_| are owned by |toplevel_menus_|.
  std::unique_ptr<ui::SimpleMenuModel> root_menu_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> toplevel_menus_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>>
      recently_closed_window_menus_;
  ui::SimpleMenuModel* history_menu_ = nullptr;
  ui::SimpleMenuModel* profiles_menu_ = nullptr;

  // Tracks value of the kShowBookmarkBar preference.
  PrefChangeRegistrar pref_change_registrar_;

  scoped_refptr<history::TopSites> top_sites_;

  sessions::TabRestoreService* tab_restore_service_;  // weak

  std::unique_ptr<AvatarMenu> avatar_menu_;

  ScopedObserver<history::TopSites, history::TopSitesObserver> scoped_observer_{
      this};

  // Maps from history item command ID to HistoryItem data.
  std::map<int, std::unique_ptr<HistoryItem>> history_items_;

  // Maps from profile item command ID to an index into |avatar_menu_|, at the
  // time the menu was created.
  std::map<int, int> profile_commands_;
  int active_profile_index_ = -1;

  base::flat_set<int> observed_commands_;

  int last_command_id_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<GlobalMenuBarX11> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GlobalMenuBarX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_X11_H_
